import numpy as np

from solar_rover_sim import OBS_DIM, SolarRoverEnv
from solar_rover_sim.policy import MLPPolicy, init_params


def test_reset_step_shapes():
    env = SolarRoverEnv(seed=0)
    obs, info = env.reset(seed=0)
    assert obs.shape == (OBS_DIM,) and info["task"] in ("goal", "sun")
    for _ in range(50):
        obs, r, term, trunc, info = env.step(np.array([0.3, 0.3]))
        assert obs.shape == (OBS_DIM,) and np.isfinite(r)
        if term or trunc:
            break


def test_driving_straight_moves_forward():
    env = SolarRoverEnv(task="goal", seed=3)
    env.reset(seed=3)
    env.obs_c = np.zeros((0, 2)); env.obs_r = np.zeros(0)       # empty arena
    env.s.x, env.s.y, env.s.th = -1.0, 0.0, 0.0
    for _ in range(20):
        env.step(np.array([1.0, 1.0]))
    assert env.s.x > -0.3          # ~0.6 m/s for 2 s minus lag


def test_spin_in_place_turns():
    env = SolarRoverEnv(task="goal", seed=4)
    env.reset(seed=4)
    env.obs_c = np.zeros((0, 2)); env.obs_r = np.zeros(0)
    env.s.x = env.s.y = 0.0; th0 = env.s.th
    for _ in range(5):
        env.step(np.array([-1.0, 1.0]))
    assert abs(((env.s.th - th0 + np.pi) % (2 * np.pi)) - np.pi) > 0.5
    assert abs(env.s.x) < 0.05 and abs(env.s.y) < 0.05


def test_policy_onnx_matches_numpy(tmp_path):
    import onnxruntime as ort
    pol = MLPPolicy(init_params(np.random.default_rng(1)))
    path = str(tmp_path / "p.onnx")
    pol.export_onnx(path)
    sess = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    x = np.random.default_rng(2).normal(size=(1, OBS_DIM)).astype(np.float32)
    a_onnx = sess.run(["action"], {"obs": x})[0][0]
    assert np.allclose(a_onnx, pol.act(x[0]), atol=1e-5)


def test_es_trainer_runs(tmp_path):
    from solar_rover_sim.train_es import main
    out = str(tmp_path / "pol")
    main(["--generations", "2", "--pop", "2", "--episodes", "1", "--workers", "1", "--out", out])
    assert (tmp_path / "pol.onnx").exists() and (tmp_path / "pol.npz").exists()
