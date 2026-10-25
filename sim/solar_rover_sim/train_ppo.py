"""Optional: train with PPO from Stable-Baselines3 instead of ES.

Needs PyTorch and SB3 (`pip install stable-baselines3`), which are large; the
default trainer (train_es.py) needs only numpy. The trained network is a
14-64-64-2 MLP. It is saved as <out>.zip (SB3) and <out>.onnx for the agent
(the agent reads both the ES .npz files and any ONNX file with an `obs` input
and `action` output).

    python -m solar_rover_sim.train_ppo --steps 2000000 --out ppo_drive
"""
import argparse


def main():
    from stable_baselines3 import PPO
    from stable_baselines3.common.env_util import make_vec_env
    import torch

    from .env import CarParams, SolarRoverEnv

    ap = argparse.ArgumentParser()
    ap.add_argument("--steps", type=int, default=2_000_000)
    ap.add_argument("--out", default="ppo_drive")
    ap.add_argument("--params", help="car_fit.json from solar_rover_sim.fit (real-car motor numbers)")
    a = ap.parse_args()
    params = CarParams.load(a.params) if a.params else None
    env = make_vec_env(lambda: SolarRoverEnv(task="mixed", params=params), n_envs=8)
    model = PPO("MlpPolicy", env, n_steps=1024, batch_size=2048, gamma=0.99, learning_rate=3e-4,
                policy_kwargs=dict(net_arch=[64, 64], activation_fn=torch.nn.Tanh), verbose=1)
    model.learn(total_timesteps=a.steps)
    model.save(a.out)

    class Actor(torch.nn.Module):
        def __init__(self, p):
            super().__init__(); self.p = p
        def forward(self, obs):
            # clamp, like SB3 does to the actions the policy was trained with
            return torch.clamp(self.p.action_net(self.p.mlp_extractor.forward_actor(self.p.extract_features(obs, self.p.pi_features_extractor))), -1, 1)

    args = (Actor(model.policy), torch.zeros(1, 14), a.out + ".onnx")
    kw = dict(input_names=["obs"], output_names=["action"], opset_version=13)
    try:   # torch >= 2.9 defaults to the dynamo exporter, which needs the extra onnxscript package
        torch.onnx.export(*args, dynamo=False, **kw)
    except TypeError:   # older torch has no dynamo argument
        torch.onnx.export(*args, **kw)
    print("saved", a.out + ".zip", a.out + ".onnx")


if __name__ == "__main__":
    main()
