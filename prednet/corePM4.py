"""corePM4.py —— corePM 的结构路由重构：只实例化被激活的一条分支。

    WholePred   （nFibers == 1）        线性截面
    LadderPred  （anchored，多纤维）    跨度门 + coreL1p 单侧阶梯
    CrossPred   （其余，多纤维）        掩码读数 + 类原型 + keep/sel 折叠
    ClsHead     （分类，单纤维）                类别头 MLP 读分类

Router 是零参数的形状判断，行为与 corePM 逐位一致；未激活的分支不建参数。
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

from .corePM import (ALPHA_MAX, ALPHA_MIN, EPS, TOL, W_MIN, mlp)


class WholePred(nn.Module):
    def __init__(self, subjDim, objDim, hidden=32):
        super().__init__()
        self.fiberSize = objDim
        self.net = mlp(subjDim, hidden, 3 * objDim)
        for mod in self.modules():
            if isinstance(mod, nn.Linear):
                nn.init.xavier_uniform_(mod.weight)
                nn.init.zeros_(mod.bias)

    def forward(self, subj, obj):
        B = len(subj)
        x = obj.view(B, 1, self.fiberSize)
        massLogits, direction, offset = self.net(subj).split(self.fiberSize, -1)
        mass = torch.softmax(massLogits, -1)
        invSq = 1.0 / mass.pow(2).sum(-1, keepdim=True).clamp(min=EPS)
        direction = direction - mass * (mass * direction).sum(-1, keepdim=True) * invSq
        offset = offset - mass * (mass * offset).sum(-1, keepdim=True) * invSq
        p = torch.einsum("bnd,bd->bn", x, mass)
        out = p.unsqueeze(-1) * (1.0 + direction.unsqueeze(1)) + offset.unsqueeze(1)
        with torch.no_grad():                                    # 成员短路：已在线上就原样返回
            near = ((x - out).pow(2).sum(-1)
                    <= TOL * TOL * (1.0 + x.pow(2).sum(-1)))
        out = torch.where(near.unsqueeze(-1), x, out)
        return out.reshape(B, -1)


class LadderPred(nn.Module):
    def __init__(self, subjDim, fiberSize, hidden=32, levels=1):
        super().__init__()
        self.fiberSize = fiberSize
        self.levels = max(1, int(levels))
        self.massNet = mlp(subjDim, hidden, fiberSize)
        self.slope = mlp(subjDim + 1, hidden, fiberSize)
        self.incScale = nn.Parameter(torch.zeros(()))
        self.alpha = nn.Parameter(torch.tensor(0.9))
        for mod in self.modules():
            if isinstance(mod, nn.Linear):
                nn.init.xavier_uniform_(mod.weight)
                nn.init.zeros_(mod.bias)
        with torch.no_grad():
            self.slope[-1].weight.zero_()
            self.slope[-1].bias.zero_()

    def ladder(self, subj):
        B = len(subj)
        i = torch.arange(self.levels + 1, device=subj.device, dtype=subj.dtype)
        rows = torch.cat([subj.unsqueeze(1).expand(-1, self.levels + 1, -1),
                          i.view(1, -1, 1).expand(B, -1, 1)], -1)
        raw = self.slope(rows)
        inc = self.incScale * (W_MIN + F.softplus(raw[:, 1:]))
        y = torch.cat([raw[:, :1], inc], 1).cumsum(1)
        skip = (i / self.levels).view(1, -1, 1) * subj.unsqueeze(1)
        return y + skip

    def forward(self, subj, x):
        B, N, D = x.shape
        mass = torch.softmax(self.massNet(subj), -1)
        p = torch.einsum("bnd,bd->bn", x, mass)
        alpha = self.alpha.clamp(ALPHA_MIN, ALPHA_MAX)
        alpha = alpha + (self.alpha - self.alpha.detach())

        with torch.no_grad():
            lo = p.min(-1, keepdim=True).values
            span = (p.max(-1, keepdim=True).values - lo).clamp(min=EPS)
            step = span / self.levels
            level = ((p - lo) / step + alpha).floor().clamp(0, self.levels).long()
        u = (p - lo) / step + alpha

        y = self.ladder(subj)
        idx = level.unsqueeze(-1).expand(-1, -1, D)
        cell = y.gather(1, idx)
        loIdx = level.clamp(0, self.levels - 1).unsqueeze(-1).expand(-1, -1, D)
        hiIdx = (level.clamp(0, self.levels - 1) + 1).unsqueeze(-1).expand(-1, -1, D)
        seg = (y.gather(1, hiIdx) - y.gather(1, loIdx)).abs().detach()
        out = cell + (u - u.detach()).unsqueeze(-1) * seg

        with torch.no_grad():
            near = ((x.unsqueeze(2) - y.unsqueeze(1)).pow(2).sum(-1).min(-1).values
                    <= TOL * TOL * (1.0 + x.pow(2).sum(-1)))
        out = torch.where(near.unsqueeze(-1), x, out)
        return out.reshape(B, -1)


class CrossPred(nn.Module):
    def __init__(self, subjDim, fiberSize, nFibers, hidden=32, levels=1,
                 beta=0.5, maxClass=16):
        super().__init__()
        self.fiberSize = fiberSize
        self.nFibers = nFibers
        self.nClass = min(max(1, int(levels)) + 1, int(maxClass))
        self.beta = float(beta)
        self.readDim = min(fiberSize, 4)
        self.maskBase = nn.Parameter(
            torch.eye(fiberSize)[:, :self.readDim].clone())
        self.maskDelta = mlp(subjDim, hidden, fiberSize * self.readDim)
        self.ctrl = mlp(subjDim, hidden, 4)
        self.subjEmb = nn.Linear(subjDim, 4, bias=False)
        self.rankEmb = nn.Parameter(torch.zeros(self.nClass, 4))
        for mod in self.modules():
            if isinstance(mod, nn.Linear):
                nn.init.xavier_uniform_(mod.weight)
                if mod.bias is not None:
                    nn.init.zeros_(mod.bias)
        with torch.no_grad():
            self.maskDelta[-1].weight.zero_()
            self.maskDelta[-1].bias.zero_()
            self.subjEmb.weight.zero_()

    @staticmethod
    def _kmeans(p, k, iters=6):
        B, N, r = p.shape
        centers = [p[:, 0]]
        for _ in range(1, k):
            dist = torch.stack(
                [(p - c.unsqueeze(1)).pow(2).sum(-1) for c in centers], 1).min(1).values
            idx = dist.argmax(-1).view(B, 1, 1).expand(B, 1, r)
            centers.append(p.gather(1, idx).squeeze(1))
        centers = torch.stack(centers, 1)
        for _ in range(iters):
            dist = (p.unsqueeze(2) - centers.unsqueeze(1)).pow(2).sum(-1)
            g = dist.argmin(-1)
            cnt = torch.zeros(B, k, device=p.device, dtype=p.dtype)
            cnt = cnt.scatter_add(1, g, torch.ones_like(g, dtype=p.dtype))
            s = torch.zeros(B, k, r, device=p.device, dtype=p.dtype)
            s = s.scatter_add(1, g.unsqueeze(-1).expand(-1, -1, r), p)
            new = s / cnt.clamp(min=1).unsqueeze(-1)
            centers = torch.where((cnt > 0).unsqueeze(-1), new, centers)
        dist = (p.unsqueeze(2) - centers.unsqueeze(1)).pow(2).sum(-1)
        return dist.argmin(-1)

    def forward(self, subj, x):
        B, N, D = x.shape
        K, r = self.nClass, self.readDim
        M = self.maskBase.unsqueeze(0) + self.maskDelta(subj).view(B, D, r)
        denom = M.sum(1)
        denom = torch.where(denom.abs() < EPS,
                            torch.full_like(denom, EPS), denom)
        p = torch.einsum("bnd,bdr->bnr", x, M) / denom.unsqueeze(1)

        with torch.no_grad():
            group = self._kmeans(p, K)
            cnt = torch.zeros(B, K, device=x.device, dtype=x.dtype)
            cnt = cnt.scatter_add(1, group, torch.ones_like(group, dtype=x.dtype))
        proto = torch.zeros(B, K, D, device=x.device, dtype=x.dtype)
        proto = proto.scatter_add(
            1, group.unsqueeze(-1).expand(-1, -1, D), x)
        proto = proto / cnt.clamp(min=1).unsqueeze(-1)
        weight = cnt / self.nFibers

        keepHead, keepW, selHead, selW = self.ctrl(subj).split(1, -1)
        keepScore = keepHead + keepW * weight
        rank = weight.argsort(-1, descending=True).argsort(-1)
        selE = (self.subjEmb(subj) @ self.rankEmb.t()).gather(1, rank)
        selScore = selHead + selW * weight + selE + self.beta * torch.log(weight + EPS)

        if self.training:
            keep = torch.sigmoid(keepScore)
            selSoft = torch.softmax(selScore / 0.5, -1)
            selCode = (proto * selSoft.unsqueeze(-1)).sum(1)
        else:
            keep = (keepScore > 0).to(x.dtype)
            selHard = F.one_hot(selScore.argmax(-1), K).to(x.dtype)
            selCode = (proto * selHard.unsqueeze(-1)).sum(1)

        own = proto.gather(1, group.unsqueeze(-1).expand(-1, -1, D))
        kg = keep.gather(1, group).unsqueeze(-1)
        out = kg * own + (1.0 - kg) * selCode.unsqueeze(1)

        if not self.training:
            with torch.no_grad():
                near = ((x - out).pow(2).sum(-1)
                        <= TOL * TOL * (1.0 + x.pow(2).sum(-1)))
                uniform = (x - x[:, :1]).abs().amax(dim=(1, 2)) <= TOL
                foldAll = keep.sum(-1) <= 0
                fix = near | (uniform & foldAll).unsqueeze(1)
            out = torch.where(fix.unsqueeze(-1), x, out)
        return out.reshape(B, -1)


class ClsHead(nn.Module):
    """单纤维、非 anchored：主词 → logits。obj 是占位符，输出与 obj 无关。"""

    def __init__(self, subjDim, outDim, hidden=128):
        super().__init__()
        self.net = mlp(subjDim, hidden, outDim)
        for mod in self.modules():
            if isinstance(mod, nn.Linear):
                nn.init.xavier_uniform_(mod.weight)
                nn.init.zeros_(mod.bias)

    def forward(self, subj, obj):
        return self.net(subj)


class PredPM4(nn.Module):
    """结构路由：按形状只建 WholePred / LadderPred / CrossPred / ClsHead 之一。"""

    def __init__(self, subjDim, objDim, hidden=32, block=None, levels=1,
                 points=17, beta=0.5, maxClass=16):
        super().__init__()
        self.fiberSize = objDim if block is None else int(block)
        if objDim % self.fiberSize:
            raise ValueError(f"objDim={objDim} 不能被 block={self.fiberSize} 整除")
        self.objDim = objDim
        self.nFibers = objDim // self.fiberSize
        self.anchored = subjDim == self.fiberSize
        self.levels = max(1, int(levels))

        if self.nFibers == 1:
            if self.anchored:                                   # 单纤维状态任务：线性截面
                self.route = "whole"
                self.branch = WholePred(subjDim, objDim, hidden)
            else:                                               # 单纤维占位宾词：分类头
                self.route = "cls"
                self.branch = ClsHead(subjDim, objDim, hidden * 2)
        elif self.anchored:
            self.route = "ladder"
            self.branch = LadderPred(subjDim, self.fiberSize, hidden, self.levels)
        else:
            self.route = "cross"
            self.branch = CrossPred(subjDim, self.fiberSize, self.nFibers,
                                    hidden, self.levels, beta, maxClass)

    def forward(self, subj, obj):
        if self.route in ("cls", "whole"):
            return self.branch(subj, obj)
        x = obj.view(len(subj), self.nFibers, self.fiberSize)
        return self.branch(subj, x)
