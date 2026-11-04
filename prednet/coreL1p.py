"""coreL1p.py —— coreL1m 的单侧版本：y[0] 是自由 base（也是格点），格点是 y[0] … y[points-1]。

"""
import torch
import torch.nn as nn
import torch.nn.functional as F

TOL = 1e-6                                          # 成员判定相对容差
W_MIN = 1e-3                                        # 段长下界，防 softplus 下溢成 0 后该段永死


def mlp(inDim, hidden, outDim):
    return nn.Sequential(
        nn.Linear(inDim, hidden), nn.LeakyReLU(0.01),
        nn.Linear(hidden, hidden), nn.LeakyReLU(0.01),
        nn.Linear(hidden, outDim),
    )


class Steer(torch.autograd.Function):

    @staticmethod
    def forward(ctx, x, slope, cell, idx, top):
        ctx.save_for_backward(x, slope, idx)
        ctx.top = top
        return cell

    @staticmethod
    def backward(ctx, g):
        x, slope, idx = ctx.saved_tensors
        above = idx > ctx.top
        below = idx < 0
        inward = torch.where(above, torch.ones_like(x), -torch.ones_like(x))
        gx = torch.where(above | below, g.abs() * inward, g * slope)
        return gx, None, g, None, None    # 格点值本身仍按 δ 更新


class PredL1P(nn.Module):
    """主词决定每维一条单侧递增链；宾词落在链上则原样返回，否则吸附到最近格点。"""

    def __init__(self, subjDim, objDim, hidden=32, points=17):
        super().__init__()
        self.points = points
        self.slope = mlp(subjDim + 1, hidden, objDim)           # i=0 自由 base，i≥1 增量
        self.index = mlp(subjDim + objDim, hidden, objDim)      # 原始输出即下标（无界）

    def ladder(self, subj):
        """(batch, points−1, objDim) 段长 + (batch, points, objDim) 格点值。

        y[0] = base（自由，也是格点），y[k] = y[0] + inc[0] + … + inc[k−1]。
        """
        batch = len(subj)
        i = torch.arange(self.points, device=subj.device, dtype=subj.dtype)
        rows = torch.cat([subj.unsqueeze(1).expand(-1, self.points, -1),
                          i.view(1, -1, 1).expand(batch, -1, 1)], -1)
        raw = self.slope(rows)                                  # (batch, points, objDim)
        inc = W_MIN + F.softplus(raw[:, 1:])                    # inc[k] = y[k+1] − y[k]
        y = torch.cat([raw[:, :1], inc], 1).cumsum(1)           # y[0] = base（自由，也是格点）
        return y, inc

    def forward(self, subj, obj):
        y, inc = self.ladder(subj)                              # inc: (B, points−1, D)，全部 > 0
        x = self.index(torch.cat([subj, obj], -1))              # 实数下标，无界
        top = self.points - 1

        with torch.no_grad():
            near = (y - obj.unsqueeze(1)).abs().amin(1)
            member = (near <= TOL * obj.abs().clamp(min=1.0)).all(-1, keepdim=True)
            idx = x.round().long()                              # 语义格号：决定吸附到哪一格

        cell = y.gather(1, idx.clamp(0, top).unsqueeze(1)).squeeze(1)
        # 格 k → k+1 的增量是 inc[k]；x<0 时 floor=−1 被夹到 0，即把第一段向左外推
        inc_idx = x.floor().long().clamp(0, top - 1).unsqueeze(1)
        slope = inc.gather(1, inc_idx).squeeze(1)
        out = Steer.apply(x, slope, cell, idx, top)     # 前向=格点值，反向=方向纠正
        return torch.where(member, obj, out)


if __name__ == "__main__":
    torch.manual_seed(0)
    subj, obj = torch.rand(8, 3), torch.rand(8, 16) * 4
    for points in (9, 17, 21):
        net = PredL1P(3, 16, hidden=16, points=points)
        with torch.no_grad():
            y = net(subj, obj)
            T, seg = net.ladder(subj)
            print(f"points={points:2d} · 幂等 {float((y - net(subj, y)).abs().max()):.1e} · "
                  f"严格递增 {bool(((T[:, 1:] - T[:, :-1]) > 0).all())} · "
                  f"格面 {tuple(T.shape)} · 段长中位 {float(seg.median()):.2e} · "
                  f"参数 {sum(p.numel() for p in net.parameters()):,}")
