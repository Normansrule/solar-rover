"""Tiny MLP policy (numpy) shared by training and the on-car agent, plus
ONNX export. obs(14) -> 32 -> 32 -> 2, tanh everywhere."""
from __future__ import annotations

import numpy as np

SIZES = (14, 32, 32, 2)


def n_params(sizes=SIZES) -> int:
    return sum(a * b + b for a, b in zip(sizes[:-1], sizes[1:]))


def unflatten(theta: np.ndarray, sizes=SIZES):
    layers, i = [], 0
    for a, b in zip(sizes[:-1], sizes[1:]):
        W = theta[i:i + a * b].reshape(a, b); i += a * b
        bias = theta[i:i + b]; i += b
        layers.append((W, bias))
    return layers


def init_params(rng: np.random.Generator, sizes=SIZES) -> np.ndarray:
    parts = []
    for a, b in zip(sizes[:-1], sizes[1:]):
        parts.append(rng.normal(0, 1 / np.sqrt(a), a * b))
        parts.append(np.zeros(b))
    return np.concatenate(parts)


class MLPPolicy:
    def __init__(self, theta: np.ndarray, sizes=SIZES):
        self.sizes = tuple(sizes)
        self.theta = np.asarray(theta, dtype=np.float64)
        self.layers = unflatten(self.theta, self.sizes)

    def act(self, obs: np.ndarray) -> np.ndarray:
        h = np.asarray(obs, dtype=np.float64)
        for W, b in self.layers:
            h = np.tanh(h @ W + b)
        return h

    def save(self, path: str):
        np.savez(path, theta=self.theta, sizes=np.array(self.sizes))

    @classmethod
    def load(cls, path: str) -> "MLPPolicy":
        d = np.load(path)
        return cls(d["theta"], tuple(int(x) for x in d["sizes"]))

    def export_onnx(self, path: str):
        import onnx
        from onnx import TensorProto, helper, numpy_helper
        nodes, inits = [], []
        prev = "obs"
        for k, (W, b) in enumerate(self.layers):
            inits += [numpy_helper.from_array(W.astype(np.float32), f"W{k}"),
                      numpy_helper.from_array(b.astype(np.float32), f"b{k}")]
            nodes.append(helper.make_node("Gemm", [prev, f"W{k}", f"b{k}"], [f"z{k}"]))
            out = "action" if k == len(self.layers) - 1 else f"h{k}"
            nodes.append(helper.make_node("Tanh", [f"z{k}"], [out]))
            prev = out
        graph = helper.make_graph(
            nodes, "solar_rover_policy",
            [helper.make_tensor_value_info("obs", TensorProto.FLOAT, [1, self.sizes[0]])],
            [helper.make_tensor_value_info("action", TensorProto.FLOAT, [1, self.sizes[-1]])],
            inits)
        model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)], producer_name="solarrover")
        model.ir_version = 8
        onnx.checker.check_model(model)
        onnx.save(model, path)
