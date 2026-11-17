Import("env")

# The strict flags you only want for Core/ code
STRICT_WARNINGS = [
    "-Wall",
    "-Wextra",
    "-Wpedantic"
]


def apply_core_warnings(env, node):
    # Replace backslashes to ensure compatibility (Windows = bad)
    path = node.get_path().replace("\\", "/")

    # Check if the source file is inside the Core/ directory
    if "Core/" in path:
        return env.Object(
            node,
            CCFLAGS=env.get("CCFLAGS", []) + STRICT_WARNINGS
        )

    return node


env.AddBuildMiddleware(apply_core_warnings)