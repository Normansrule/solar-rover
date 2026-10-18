import pytest

mujoco = pytest.importorskip("mujoco")


def test_model_loads_and_drives():
    import math
    import pathlib
    m = mujoco.MjModel.from_xml_path(str(pathlib.Path(__file__).parent.parent / "models" / "solar_rover.xml"))
    d = mujoco.MjData(m)
    for _ in range(1500):
        d.ctrl[0:4] = 8.0
        d.ctrl[5] = math.radians(45)
        mujoco.mj_step(m, d)
    assert d.body("chassis").xpos[0] > 0.1
    assert d.warning[mujoco.mjtWarning.mjWARN_BADQACC].number == 0
