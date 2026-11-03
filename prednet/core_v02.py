import math
import torch
import torch.nn as nn
import torch.nn.functional as F

__all__ = ["Pred", "Fused", "fold", "invert", "invert_obj"]

TAU = 0.1                                           # 软门控温度
HALF_INIT = 0.7                                     # 初始半宽
LEAK = 0.01                                         # 门控泄漏下界：区间外仍有梯度


class Pred(nn.Module):
    def __init__(self, subjDim, objDim, hidden=32, ranges=1):
        super().__init__()
        self.subjDim, self.objDim = subjDim, objDim
        self.hidden, self.ranges = hidden, ranges
        self.net = nn.Sequential(
            nn.Linear(subjDim, hidden), nn.LeakyReLU(0.01),
            nn.Linear(hidden, hidden), nn.LeakyReLU(0.01),
            nn.Linear(hidden, (1 + 2 * ranges) * objDim),  # core + ranges×(center, logHalf)
        )
        with torch.no_grad():
            outLayer = self.net[-1]
            outLayer.bias[:objDim].zero_()                # core bias
            rangeBias = outLayer.bias[objDim:].view(ranges, 2, objDim)
            rangeBias[:, 0].zero_()                       # center = 0
            rangeBias[:, 1].fill_(
                math.log(math.expm1(HALF_INIT)))           # halfWidth ≈ HALF_INIT

    def forward(self, subj, obj):
        fields = self.net(subj).view(len(subj), 1 + 2 * self.ranges, self.objDim)
        core = fields[:, 0]
        center, logHalf = fields[:, 1::2], fields[:, 2::2]  # 独立的区间中心

        isInside = torch.zeros_like(obj, dtype=torch.bool)
        noHit = torch.ones_like(obj)                      # ∏(1 − softHit[j])
        for j in range(self.ranges):
            rangeCenter = center[:, j]
            halfWidth = F.softplus(logHalf[:, j])
            lo, hi = rangeCenter - halfWidth, rangeCenter + halfWidth
            isInside = isInside | ((obj >= lo) & (obj <= hi))
            softHit = (torch.sigmoid((obj - lo) / TAU)
                       * torch.sigmoid((hi - obj) / TAU))
            noHit = noHit * (1.0 - softHit)

        cons = torch.where(isInside, core, obj)
        gate = (1.0 - noHit + LEAK).clamp(max=1.0)        # 概率并 + 泄漏
        smooth = obj + gate * (core - obj)                 # 光滑插值
        return cons.detach() + (smooth - smooth.detach())  # 前向取 cons，反向经 smooth 求导


class Fused(nn.Module):
    def __init__(self, pred, lo, hi, frozenCore):
        super().__init__()
        self.downstream = pred
        self.register_buffer("frozenCore", frozenCore)
        self.register_buffer("frozenLo", lo)
        self.register_buffer("frozenHi", hi)

    def forward(self, subj, obj):
        liveCore, center, logHalf = self.downstream.net(subj).split(
            self.downstream.objDim, dim=-1)
        halfWidth = F.softplus(logHalf)
        lo, hi = center - halfWidth, center + halfWidth
        core = torch.where((self.frozenCore >= lo) & (self.frozenCore <= hi),
                           liveCore, self.frozenCore)
        isInside = ((obj >= self.frozenLo) & (obj <= self.frozenHi)) | \
                   ((obj >= lo) & (obj <= hi))
        return torch.where(isInside, core, obj)


def fold(predUp, predDown, subj):
    subj = subj.detach()
    with torch.no_grad():
        core, center, logHalf = predUp.net(subj).split(predUp.objDim, dim=-1)
        halfWidth = F.softplus(logHalf)
        return Fused(predDown, center - halfWidth, center + halfWidth, core)


def invert(pred, subj, obj, target, iters=60, lr=0.2):
    subj, obj, target = subj.detach(), obj.detach(), target.detach()
    identityMask = (obj == target).float()                       # 恒等支掩码：obj 本身即目标
    delta = torch.zeros_like(subj, requires_grad=True)
    opt = torch.optim.Adam([delta], lr=lr)
    for _ in range(iters):
        core, center, logHalf = pred.net(subj + delta).split(pred.objDim, dim=-1)
        halfWidth = F.softplus(logHalf)
        lo, hi = center - halfWidth, center + halfWidth
        outsideAmount = F.relu(lo - obj) + F.relu(obj - hi)      # 到区间的域外量
        insideDepth = torch.minimum(obj - lo, hi - obj)          # 区间对 obj 的域内深度
        penalty = (1 - identityMask) * ((core - target).abs() + outsideAmount) \
            + identityMask * F.relu(insideDepth)
        grad, = torch.autograd.grad(penalty.pow(2).sum(), [delta])
        delta.grad = grad
        opt.step()
    with torch.no_grad():
        return {"subj": (subj + delta).detach(),
                "resid": (pred(subj + delta, obj) - target).abs().mean(1)}


def invert_obj(pred, subj, cons, mode="center"):
    if mode not in ("center", "random"):
        raise ValueError(f"mode 必须是 center / random，收到 {mode!r}")
    subj, cons = subj.detach(), cons.detach()
    with torch.no_grad():
        core, center, logHalf = pred.net(subj).split(pred.objDim, dim=-1)
        halfWidth = F.softplus(logHalf)
        lo, hi = center - halfWidth, center + halfWidth
        isConstructed = cons == core
        if mode == "center":
            fillValue = center
        else:
            fillValue = lo + (hi - lo) * torch.rand_like(lo)
        return torch.where(isConstructed, fillValue, cons)
