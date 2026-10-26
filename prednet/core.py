import math

import torch
import torch.nn as nn
import torch.nn.functional as F

__all__ = ["Pred", "Combo", "CodeHead", "DAG", "Fused", "fold", "invert"]

TAU = 0.1                                           # 软门控温度
MARGIN = 0.1                                        # 码字打分 margin
GAP_MIN = 1.0                                       # gap ∈ (−1, ∞)
GAP_BIAS = math.log(math.expm1(GAP_MIN))            # 初始化 gap = 0
WID_SPAN = 0.7                                      # 每侧初始总宽


def _mlp(i, h, o):
    return nn.Sequential(nn.Linear(i, h), nn.ReLU(), nn.Linear(h, o))


class Pred(nn.Module):
    def __init__(self, sd, od, r=1, h=32):
        super().__init__()
        self.sd, self.od, self.r = sd, od, r            # sd 主词(环境)维，od 宾词(被构造)维
        self.core = _mlp(sd, h, od)
        self.wid = nn.ModuleList(_mlp(sd, h, r * od) for _ in range(2))
        self.gap = nn.ModuleList(_mlp(sd, h, (r - 1) * od) for _ in range(2)) if r > 1 else None
        for hd in self.wid:
            nn.init.constant_(hd[-1].bias, math.log(math.expm1(WID_SPAN / r)))
        if self.gap is not None:
            for hd in self.gap:
                nn.init.constant_(hd[-1].bias, GAP_BIAS)

    def rng(self, subj, c=None):
        b, r = len(subj), self.r
        c = (self.core(subj) if c is None else c).unsqueeze(1)
        z = c.new_zeros(b, 1, self.od)
        w = [F.softplus(hd(subj)).view(b, r, self.od) for hd in self.wid]
        g = [torch.cat([z, F.softplus(hd(subj)).view(b, r - 1, self.od) - GAP_MIN], 1)
             for hd in self.gap] if self.gap is not None else [z.expand(b, r, self.od)] * 2
        hi_pos = c + torch.cumsum(w[0] + g[0], 1)                 # 正侧右端
        lo_neg = c - torch.cumsum(w[1] + g[1], 1)                 # 负侧左端
        lo = torch.cat([lo_neg.flip(1), hi_pos - w[0]], 1)
        hi = torch.cat([lo_neg.flip(1) + w[1].flip(1), hi_pos], 1)
        return lo, hi

    def forward(self, subj, obj):
        c = self.core(subj)
        lo, hi = self.rng(subj, c)
        x = obj.unsqueeze(1)
        hard = torch.where(((x >= lo) & (x <= hi)).any(1), c, obj)
        p = torch.sigmoid((x - lo) / TAU) * torch.sigmoid((hi - x) / TAU)
        soft = obj + (1 - torch.prod(1 - p, 1)) * (c - obj)       # 概率并（缝隙里也有梯度）
        return hard.detach() + (soft - soft.detach())             # 前向 hard，反向 soft


class Combo(nn.Module):
    def __init__(self, ad, bd, d, r=1, h=32):
        super().__init__()
        self.grid, self.side = int(round(math.sqrt(ad))), int(round(math.sqrt(d)))
        self.p1 = Pred(ad, d, r, h)                               # 环境 a → 构造 d 维
        self.p2 = Pred(bd, d, r, h)                               # 环境 b → 构造 d 维
        self.p3 = Pred(d, d, r, h)

    def forward(self, a, b):
        s = F.adaptive_avg_pool2d(a.view(-1, 1, self.grid, self.grid),
                                  (self.side, self.side)).flatten(1)
        c1, c2 = self.p1(a, s), self.p2(b, s)
        return self.p3(c2, c1)                                    # 环境 c2，构造 c1


class CodeHead(nn.Module):

    def __init__(self, codes, sd, r=1, h=32):
        super().__init__()
        self.register_buffer("codes", torch.as_tensor(codes).float())
        self.p = Pred(sd, self.codes.shape[1], r, h)              # 环境 sd 维 → 构造码字
        self.scale = nn.Parameter(torch.tensor(4.0))

    def forward(self, subj):
        lo, hi = self.p.rng(subj)
        c = self.codes.unsqueeze(1)
        d = (F.relu(lo.unsqueeze(1) + MARGIN - c)
             + F.relu(c - hi.unsqueeze(1) + MARGIN)).amin(2)
        return -F.softplus(d / 0.05).mean(-1) * F.softplus(self.scale)


class DAG(nn.Module):
    def __init__(self, ad, bd, d, codes, k=2, h=32, r=1):
        super().__init__()
        self.units = nn.ModuleList(Combo(ad, bd, d, r, h) for _ in range(k))
        self.merges = nn.ModuleList(Pred(d, d, r, h) for _ in range(k - 1))
        self.head = CodeHead(codes, d, r, h)

    def forward(self, a, bands):
        hh = self.units[0](a, bands[0])
        for m, u, b in zip(self.merges, self.units[1:], bands[1:]):
            hh = m(u(a, b), hh)                                   # 环境 = u 的输出
        return self.head(hh)


class Fused(nn.Module):

    def __init__(self, p, lo, hi, rep):
        super().__init__()
        self.p = p
        self.register_buffer("rep0", rep)
        self.register_buffer("lo", lo)
        self.register_buffer("hi", hi)

    def forward(self, subj, obj):
        lo, hi = self.p.rng(subj)
        rep = self.p(subj, self.rep0)
        x = obj.unsqueeze(1)
        inside = (((x >= self.lo) & (x <= self.hi)).any(1)
                  | ((x >= lo) & (x <= hi)).any(1))
        return torch.where(inside, rep, obj)


def fold(p_up, p_down, subj):
    subj = subj.detach()
    with torch.no_grad():
        lo, hi = p_up.rng(subj)
        return Fused(p_down, lo, hi, p_up.core(subj))


def invert(p, subj, obj, target, iters=60, lr=0.2):

    subj, obj, target = subj.detach(), obj.detach(), target.detach()
    keep = (obj == target).float()
    x = obj.unsqueeze(1)
    d = torch.zeros_like(subj, requires_grad=True)
    opt = torch.optim.Adam([d], lr=lr)
    for _ in range(iters):
        c = p.core(subj + d)
        lo, hi = p.rng(subj + d, c)
        out = (F.relu(lo - x) + F.relu(x - hi)).amin(1)           # obj 到并集的域外量
        ins = torch.minimum(x - lo, hi - x).amin(1)               # 最含它的段的域内深度
        pen = (1 - keep) * ((c - target).abs() + out) + keep * F.relu(ins)
        grad, = torch.autograd.grad(pen.pow(2).sum(), [d])
        d.grad = grad
        opt.step()
    with torch.no_grad():
        return {"subj": (subj + d).detach(),
                "resid": (p(subj + d, obj) - target).abs().mean(1)}
