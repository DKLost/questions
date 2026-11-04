"""coreL1m.py —— 三个 MLP 的一元格点谓词（最简实现）。

    base   subj → objDim                 中点锚（y 轴）
    slope  (subj, x) → objDim            段 (x → x+1) 的斜率 dy/dx，softplus 后 ≥ W_MIN
    index  (subj, obj) → objDim          原始输出即 x 轴坐标（无界）

    y[x]  = base + 两侧前缀和(段长)      沿 x 严格递增的格点链
    F(obj) = obj                         obj 已逐维落在链上
           = y[round(x)]                 否则吸附到最近整数点

反向用解析斜率做直通：out = y[round(x)] + seg[floor(x)]·(x − x.detach())。
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


class PredL1M(nn.Module):
    """主词决定每维一条格点链；宾词落在链上则原样返回，否则吸附到最近的格点。"""

    def __init__(self, subjDim, objDim, hidden=32, points=17):
        super().__init__()
        self.points = points                                    # 供外部查询
        self.base = mlp(subjDim, hidden, objDim)                # 中点锚（y 轴）
        self.slope = mlp(subjDim + 1, hidden, objDim)           # 段长 = dy/dx
        self.index = mlp(subjDim + objDim, hidden, objDim)      # 原始输出 = x 轴坐标

    def ladder(self, subj):
        """(batch, points, objDim) 格点值与段长，链沿 x 严格递增。"""
        batch, middle = len(subj), self.points // 2
        x = torch.arange(self.points, device=subj.device, dtype=subj.dtype) - middle
        rows = torch.cat([subj.unsqueeze(1).expand(-1, self.points, -1),
                          x.view(1, -1, 1).expand(batch, -1, 1)], -1)
        seg = W_MIN + F.softplus(self.slope(rows))             # 段 (x → x+1) 的斜率 dy/dx
        y = torch.cat([-seg[:, :middle].flip(1).cumsum(1).flip(1),
                       torch.zeros_like(seg[:, :1]),
                       seg[:, middle:-1].cumsum(1)], 1)
        return y + self.base(subj).unsqueeze(1), seg

    def forward(self, subj, obj):
        y, seg = self.ladder(subj)
        x = self.index(torch.cat([subj, obj], -1))              # 原始输出即 x 轴坐标（无界）
        middle = self.points // 2

        with torch.no_grad():
            near = (y - obj.unsqueeze(1)).abs().amin(1)         # 逐维最近格点距离
            member = (near <= TOL * obj.abs().clamp(min=1.0)).all(-1, keepdim=True)
            u = x + middle                                      # 坐标 → 数组下标
            idx = u.round().long().clamp(0, self.points - 1).unsqueeze(1)      # 前向：最近格点
            seg_idx = u.floor().clamp(0, self.points - 2).long().unsqueeze(1)  # 反向：所在段

        # 前向 = 格点值；反向 = 该处斜率（左侧值负但增量恒正，无需翻符号）
        out = y.gather(1, idx).squeeze(1) + seg.gather(1, seg_idx).squeeze(1) * (x - x.detach())
        return torch.where(member, obj, out)
