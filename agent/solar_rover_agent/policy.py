"""Load a trained policy. Prefers the numpy .npz (no extra packages); falls
back to onnxruntime for .onnx files (e.g. from the PPO trainer)."""
from __future__ import annotations

import pathlib

import numpy as np


class Policy:
    def __init__(self, path: str):
        p = pathlib.Path(path)
        self.kind = None
        if p.suffix == ".npz" or (p.with_suffix(".npz").exists() and p.suffix != ".onnx"):
            d = np.load(p.with_suffix(".npz"))
            theta, sizes = d["theta"], [int(x) for x in d["sizes"]]
            self.layers, i = [], 0
            for a, b in zip(sizes[:-1], sizes[1:]):
                W = theta[i:i + a * b].reshape(a, b); i += a * b
                bias = theta[i:i + b]; i += b
                self.layers.append((W, bias))
            self.kind = "numpy"
        else:
            import onnxruntime as ort
            self.sess = ort.InferenceSession(str(p), providers=["CPUExecutionProvider"])
            self.kind = "onnx"

    def act(self, obs: np.ndarray) -> np.ndarray:
        if self.kind == "numpy":
            h = obs.astype(np.float64)
            for W, b in self.layers:
                h = np.tanh(h @ W + b)
            return h
        return self.sess.run(["action"], {"obs": obs.reshape(1, -1).astype(np.float32)})[0][0]
