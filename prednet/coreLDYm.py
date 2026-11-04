"""coreLDYm.py —— 一元 / 二元 / 三元

    axes=1  T[i]     = base(subj)      + 两侧阶梯 seg(subj, i)
    axes=2  T[i, j]  = base(subj, j)   + 两侧阶梯 seg(subj, i, j)
    axes=3  T[i,j,k] = base(subj, j,k) + 两侧阶梯 seg(subj, i, j, k)

python -m prednet.coreLDYm
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

TOL = 1e-6                                          # 成员判定相对容差
W_MIN = 1e-3                                        # 段长下界


def mlp(inDim, hidden, outDim):
    return nn.Sequential(
        nn.Linear(inDim, hidden), nn.LeakyReLU(0.01),
        nn.Linear(hidden, hidden), nn.LeakyReLU(0.01),
        nn.Linear(hidden, outDim),
    )


def twoSided(seg, mid):
    """(B, P, …, D) 的段长 → 中点为 0 的两侧阶梯，(B, P, …, D)。"""
    return torch.cat([-seg[:, :mid].flip(1).cumsum(1).flip(1),
                      torch.zeros_like(seg[:, :1]),
                      seg[:, mid:-1].cumsum(1)], 1)


def cell(k, mid, n):
    """坐标 → 最近整数格号（前向）。"""
    return (k + mid).round().long().clamp(0, n - 1)


def cellSeg(k, mid, n):
    """坐标 → 所在段的左端点（反向，分段线性的真实斜率）。"""
    return (k + mid).floor().clamp(0, max(n - 2, 0)).long()


def bound(raw, mid, n):
    """σ 把额外轴坐标限死在 [−mid, n−1−mid]。x 不这样做，见文件头。"""
    return -mid + (n - 1 - mid) * torch.sigmoid(raw)


def pick(t, ix, iy, iz, P, Py, Pz):
    """t (B, P, Py, Pz, D) 按三个整数下标 gather → (B, D)。"""
    flat = t.reshape(len(t), P * Py * Pz, -1)
    return flat.gather(1, ((ix * Py + iy) * Pz + iz).unsqueeze(1)).squeeze(1)


class PredLDYM(nn.Module):
    """主词决定一叠格点；宾词是格点则原样返回，否则落到 index 指派的格元。"""

    def __init__(self, subjDim, objDim, hidden=32, points=17, pointsY=3, layers=2,
                 axes=1, fixextra=False, fix=()):
        super().__init__()
        if axes not in (1, 2, 3):
            raise ValueError("axes 只能是 1 / 2 / 3")
        self.Px, self.Py, self.Pz, self.axes = points, max(1, pointsY), max(1, layers), axes
        self.fix = {"y", "z"} if fixextra else set(fix)   # 固定哪些额外轴（退化对照）
        self.base = mlp(subjDim + max(0, axes - 1), hidden, objDim)
        self.slope = mlp(subjDim + axes, hidden, objDim)
        nOut = objDim + (objDim if axes >= 2 else 0) + (1 if axes >= 3 else 0)
        self.index = mlp(subjDim + objDim, hidden, nOut)

    def grid(self, subj):
        """(T, seg)：格点值与「沿 x 的段长」，形状都是 (B, Px, Py, Pz, D)。"""
        B, P, Py, Pz, dev, dt = len(subj), self.Px, self.Py, self.Pz, subj.device, subj.dtype
        cx = torch.arange(P, device=dev, dtype=dt) - P // 2
        cy = torch.arange(Py, device=dev, dtype=dt) - Py // 2
        cz = torch.arange(Pz, device=dev, dtype=dt) - Pz // 2
        subs = subj.unsqueeze(1).expand(-1, P, -1)

        if self.axes == 1:
            rows = torch.cat([subs, cx.view(1, -1, 1).expand(B, -1, 1)], -1)
            seg = W_MIN + F.softplus(self.slope(rows))
            T = twoSided(seg, P // 2) + self.base(subj)[:, None]
            return T.view(B, P, 1, 1, -1), seg.view(B, P, 1, 1, -1)

        if self.axes == 2:
            pts = torch.cartesian_prod(cx, cy).reshape(P, Py, 2)
            rows = torch.cat([subj.unsqueeze(1).expand(-1, P * Py, -1),
                              pts.reshape(1, -1, 2).expand(B, -1, -1)], -1)
            seg = (W_MIN + F.softplus(self.slope(rows))).view(B, P, Py, -1)
            rowsB = torch.cat([subj.unsqueeze(1).expand(-1, Py, -1),
                               cy.view(1, -1, 1).expand(B, -1, 1)], -1)
            T = twoSided(seg, P // 2) + self.base(rowsB).view(B, 1, Py, -1)
            return T.view(B, P, Py, 1, -1), seg.view(B, P, Py, 1, -1)

        pts = torch.cartesian_prod(cx, cy, cz).reshape(P, Py, Pz, 3)
        rows = torch.cat([subj.unsqueeze(1).expand(-1, P * Py * Pz, -1),
                          pts.reshape(1, -1, 3).expand(B, -1, -1)], -1)
        seg = (W_MIN + F.softplus(self.slope(rows))).view(B, P, Py, Pz, -1)
        ptsB = torch.cartesian_prod(cy, cz).reshape(Py, Pz, 2)
        rowsB = torch.cat([subj.unsqueeze(1).expand(-1, Py * Pz, -1),
                           ptsB.reshape(1, -1, 2).expand(B, -1, -1)], -1)
        T = twoSided(seg, P // 2) + self.base(rowsB).view(B, 1, Py, Pz, -1)
        return T, seg

    def forward(self, subj, obj):
        B, D = len(subj), obj.shape[-1]
        T, seg = self.grid(subj)
        P, Py, Pz = T.shape[1:4]
        mid = (P // 2, Py // 2, Pz // 2)

        idx = self.index(torch.cat([subj, obj], -1))
        kx = idx[:, :D]                                         # x：无界（被积分，斜率小）
        zero = torch.zeros(B, D, device=subj.device, dtype=subj.dtype)
        ky = idx[:, D:2 * D] if self.axes >= 2 else zero
        kz = idx[:, 2 * D:] if self.axes >= 3 else zero[:, :1]
        if self.axes >= 2:                                      # y/z：σ 有界，防坐标发散
            ky = bound(ky, mid[1], Py)
        if self.axes >= 3:
            kz = bound(kz, mid[2], Pz)
        if self.axes >= 2 and "y" in self.fix:           # 退化：固定 y = 中点
            ky = ky * 0
        if self.axes >= 3 and "z" in self.fix:           # 退化：固定 z = 中点
            kz = kz * 0

        with torch.no_grad():
            cells = T.reshape(B, P * Py * Pz, D)
            near = (cells - obj[:, None, :]).abs().amin(1)
            member = (near <= TOL * obj.abs().clamp(min=1.0)).all(-1, keepdim=True)
            ix, iy, iz = cell(kx, mid[0], P), cell(ky, mid[1], Py), cell(kz, mid[2], Pz)
            jx = cellSeg(kx, mid[0], P)
            jy = cellSeg(ky, mid[1], Py) if self.axes >= 2 else iy
            jz = cellSeg(kz, mid[2], Pz) if self.axes >= 3 else iz

        out = pick(T, ix, iy, iz, P, Py, Pz)                        # 前向：格点值
        out = out + pick(seg, jx, iy, iz, P, Py, Pz) * (kx - kx.detach())   # 沿 x 的斜率
        if self.axes >= 2:                                          # 沿 y 的斜率（差分）
            dy = (pick(T, ix, (jy + 1).clamp(max=Py - 1), iz, P, Py, Pz)
                  - pick(T, ix, jy, iz, P, Py, Pz))
            out = out + dy * (ky - ky.detach())
        if self.axes >= 3:                                          # 沿 z 的斜率（差分）
            dz = (pick(T, ix, iy, (jz + 1).clamp(max=Pz - 1), P, Py, Pz)
                  - pick(T, ix, iy, jz, P, Py, Pz))
            out = out + dz * (kz - kz.detach())
        return torch.where(member, obj, out)


if __name__ == "__main__":
    torch.manual_seed(0)
    subj, obj = torch.rand(8, 3), torch.rand(8, 16) * 4
    for axes, fix in ((1, False), (2, True), (3, True), (2, False), (3, False)):
        net = PredLDYM(3, 16, hidden=16, points=9, pointsY=3, layers=2, axes=axes, fixextra=fix)
        with torch.no_grad():
            y = net(subj, obj)
            cells = net.grid(subj)[0].reshape(len(subj), -1, 16)
            snap = (cells - y[:, None, :]).abs().amin(1).max()     # 输出必须逐维落在格面上
            print(f"axes={axes} fix={int(fix)} · 幂等 {float((y - net(subj, y)).abs().max()):.1e} · "
                  f"吸附 {float(snap):.1e} · 格面 {tuple(net.grid(subj)[0].shape)} · "
                  f"参数 {sum(p.numel() for p in net.parameters()):,}")

    from prednet.coreL1m import PredL1M
    m = PredL1M(3, 16, hidden=16, points=9)
    d = PredLDYM(3, 16, hidden=16, points=9, axes=1)
    d.load_state_dict(m.state_dict())
    sweep = torch.linspace(-6, 12, 401).unsqueeze(-1).expand(-1, 16).contiguous()
    s = subj[:1].expand(401, -1)
    with torch.no_grad():
        print(f"axes=1 vs coreL1m · max|diff| "
              f"{float((m(s, sweep) - d(s, sweep)).abs().max()):.1e}")
