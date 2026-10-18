import sys

import numpy as np
import pytest

pytest.importorskip("stable_baselines3")   # optional: needs PyTorch
ort = pytest.importorskip("onnxruntime")


def test_ppo_onnx_matches_sb3(tmp_path, monkeypatch):
    from stable_baselines3 import PPO
    from solar_rover_sim import train_ppo
    from solar_rover_sim.env import SolarRoverEnv

    out = str(tmp_path / "ppo")
    monkeypatch.setattr(sys, "argv", ["train_ppo", "--steps", "1", "--out", out])
    train_ppo.main()
    model = PPO.load(out + ".zip")
    sess = ort.InferenceSession(out + ".onnx", providers=["CPUExecutionProvider"])
    obs, _ = SolarRoverEnv(task="mixed").reset(seed=0)
    onnx_act = sess.run(["action"], {"obs": obs.reshape(1, -1).astype(np.float32)})[0][0]
    sb3_act, _ = model.predict(obs, deterministic=True)
    assert np.allclose(onnx_act, sb3_act, atol=1e-5)
