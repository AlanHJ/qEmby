"""Regression checks for release-source preprocessing, without compiling."""
from prepare_source import release_remover


remover = release_remover()
sample = '''// Copyright 2024 Example (https://example.com)
const char *s = R"tag(//keep /*keep*/ "quotes")tag"; //remove
int/**/x = 1'000;
const char *url = "https://example.com";
'''
result = remover.remove_c_style_comments(sample)
assert "https://example.com)" in result
assert 'R"tag(//keep /*keep*/ "quotes")tag"' in result
assert "1'000" in result
assert "//remove" not in result
assert "int x" in result
assert '"https://example.com"' in result
assert ".ts" not in remover.supported_extensions
assert ".m" not in remover.supported_extensions
print("Comment-removal safeguards OK")
