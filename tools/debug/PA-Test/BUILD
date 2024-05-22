load("@io_bazel_rules_webtesting//web:py.bzl", "py_web_test_suite")


py_web_test_suite(
    name = "my_web_test",
    srcs = ["my_web_test.py"],
    browsers = [
        "@io_bazel_rules_webtesting//browsers:chromium-local",
    ],
    python_version = "PY3",
    local = True,
    deps = ["@io_bazel_rules_webtesting//testing/web"],
)