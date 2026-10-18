import sys, pathlib
root = pathlib.Path(__file__).parent
sys.path.insert(0, str(root))
sys.path.insert(0, str(root.parent / "sim"))
