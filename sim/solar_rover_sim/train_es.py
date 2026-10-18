"""Train the driving policy with Evolution Strategies (OpenAI-ES).

ES is a reinforcement-learning method that needs no GPU and no deep-learning
framework: it perturbs the policy weights, runs episodes in the simulator and
moves the weights toward the perturbations that earned more reward. It runs on
a laptop (or slowly on the Pi itself) with only numpy.

    python -m solar_rover_sim.train_es --generations 300 --out ../agent/models/drive
"""
from __future__ import annotations

import argparse
import json
import os
import time
from multiprocessing import Pool

import numpy as np

from .env import SolarRoverEnv
from .policy import MLPPolicy, init_params, n_params


def rollout(theta, seed, task="mixed", episodes=2):
    env = SolarRoverEnv(task=task, seed=seed)
    pol = MLPPolicy(theta)
    total = 0.0
    for e in range(episodes):
        obs, _ = env.reset(seed=seed * 7919 + e)
        done = False
        while not done:
            obs, r, term, trunc, _ = env.step(pol.act(obs))
            total += r
            done = term or trunc
    return total / episodes


def _worker(args):
    return rollout(*args)


def centered_ranks(x):
    r = np.empty(len(x)); r[np.argsort(x)] = np.arange(len(x))
    return r / (len(x) - 1) - 0.5


def evaluate(theta, task, n=20, seed0=10_000):
    stats = {"reward": [], "success": 0, "collisions": 0}
    for i in range(n):
        env = SolarRoverEnv(task=task, seed=seed0 + i)
        pol = MLPPolicy(theta)
        obs, _ = env.reset(seed=seed0 + i)
        done, tot, info = False, 0.0, {}
        while not done:
            obs, r, term, trunc, info = env.step(pol.act(obs)); tot += r; done = term or trunc
        stats["reward"].append(tot)
        stats["collisions"] += int(info.get("collided", False))
        if task == "goal":
            stats["success"] += int(info["goal_dist"] < 0.15)
        else:
            stats["success"] += int(info["light"] > 0.5)
    stats["reward"] = float(np.mean(stats["reward"]))
    return stats


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--generations", type=int, default=300)
    ap.add_argument("--pop", type=int, default=32, help="perturbation pairs per generation")
    ap.add_argument("--sigma", type=float, default=0.05)
    ap.add_argument("--lr", type=float, default=0.03)
    ap.add_argument("--episodes", type=int, default=2)
    ap.add_argument("--task", default="mixed", choices=["mixed", "goal", "sun"])
    ap.add_argument("--workers", type=int, default=os.cpu_count())
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--init", help="start from an existing .npz policy")
    ap.add_argument("--out", default="policy")
    args = ap.parse_args(argv)

    rng = np.random.default_rng(args.seed)
    theta = MLPPolicy.load(args.init).theta if args.init else init_params(rng)
    m = np.zeros_like(theta); v = np.zeros_like(theta)
    b1, b2, eps_adam = 0.9, 0.999, 1e-8
    print(f"ES: {n_params()} parameters, {args.pop * 2} rollouts x {args.episodes} episodes per generation")
    pool = Pool(args.workers) if args.workers > 1 else None
    best, history = -1e9, []
    t0 = time.time()
    for g in range(1, args.generations + 1):
        noise = rng.normal(size=(args.pop, theta.size))
        cands = np.concatenate([theta + args.sigma * noise, theta - args.sigma * noise])
        seeds = rng.integers(0, 1_000_000, args.pop)
        jobs = [(c, int(seeds[i % args.pop]), args.task, args.episodes) for i, c in enumerate(cands)]
        rewards = np.array(pool.map(_worker, jobs) if pool else list(map(_worker, jobs)))
        ranks = centered_ranks(rewards)
        grad = (ranks[:args.pop] - ranks[args.pop:]) @ noise / (2 * args.pop * args.sigma)
        grad -= 0.005 * theta                       # weight decay
        m = b1 * m + (1 - b1) * grad; v = b2 * v + (1 - b2) * grad ** 2
        theta = theta + args.lr * (m / (1 - b1 ** g)) / (np.sqrt(v / (1 - b2 ** g)) + eps_adam)
        mean_r = float(rewards.mean())
        history.append(mean_r)
        if g % 10 == 0 or g == args.generations:
            ev = evaluate(theta, "goal", n=10)
            es = evaluate(theta, "sun", n=10) if args.task != "goal" else {"success": 0}
            score = ev["success"] + es["success"]
            print(f"gen {g:4d}  mean reward {mean_r:7.2f}  goal reached {ev['success']}/10  "
                  f"found sun {es['success']}/10  crashes {ev['collisions']}/10  ({time.time() - t0:.0f}s)", flush=True)
            if score >= best:
                best = score
                MLPPolicy(theta).save(args.out + ".npz")
    pol = MLPPolicy.load(args.out + ".npz")
    pol.export_onnx(args.out + ".onnx")
    final = {"goal": evaluate(pol.theta, "goal", 30), "sun": evaluate(pol.theta, "sun", 30)}
    with open(args.out + ".json", "w") as f:
        json.dump({"generations": args.generations, "eval_30_episodes": final, "history": history}, f, indent=1)
    print("saved", args.out + ".npz", args.out + ".onnx", "eval:", json.dumps(final))


if __name__ == "__main__":
    main()
