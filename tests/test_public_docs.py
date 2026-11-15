"""Validate public-entry Markdown links and contributor onboarding files without network calls.

Historical docs are intentionally excluded: their links/claims belong to old tags.
"""
from pathlib import Path
from urllib.parse import unquote
import re

ROOT = Path(__file__).resolve().parent.parent
REQUIRED = [
    'README.md', 'CONTRIBUTING.md', 'CODE_OF_CONDUCT.md', 'SECURITY.md',
    'SUPPORT.md', 'LICENSE', 'CHANGELOG.md', 'SOURCE_MANIFEST.sha256',
    'firmware/README.md', 'android/README.md', 'web/README.md',
    'docs/README.md', 'docs/ARCHITECTURE.md', 'docs/GITHUB_RELEASE_NOTES.md',
    'docs/ROADMAP.md', 'docs/PUBLICATION_VERIFICATION.md', '.editorconfig', 'docs/DEVELOPMENT.md',
    'docs/HARDWARE_SETUP.md', 'docs/VERSIONS.md', 'docs/TROUBLESHOOTING.md',
    'docs/TESTING.md', 'docs/KNOWN_LIMITATIONS.md', 'docs/RELEASE_CHECKLIST.md',
    '.github/PULL_REQUEST_TEMPLATE.md', '.github/ISSUE_TEMPLATE/bug_report.yml',
    '.github/ISSUE_TEMPLATE/feature_request.yml',
    '.github/workflows/host-checks.yml', '.github/workflows/android-build.yml',
    '.github/workflows/firmware-build.yml'
]
checks = 0
for filename in REQUIRED:
    assert (ROOT / filename).exists(), f'missing publication file: {filename}'
    checks += 1
for filename in REQUIRED:
    path = ROOT / filename
    if path.suffix != '.md':
        continue
    content = path.read_text(encoding='utf-8')
    # Exclude fenced source blocks from hyperlink checks.
    content = re.sub(r'(?ms)^```[^\n]*\n.*?^```\s*$', '', content)
    for match in re.finditer(r'(?<!!)\[[^\]\n]+\]\(([^)]+)\)', content):
        target = match.group(1).split(' ', 1)[0].strip('<>')
        if not target or target.startswith(('https://','http://','mailto:','#')):
            continue
        target = unquote(target.split('#', 1)[0].split('?', 1)[0])
        output = (path.parent / target).resolve()
        if not output.is_relative_to(ROOT):
            raise AssertionError(f'link escapes repository: {filename} → {target}')
        if not output.exists():
            raise AssertionError(f'broken relative Markdown link: {filename} → {target}')
        checks += 1
root_readme=(ROOT/'README.md').read_text()
for phrase in ('Auto', '−100', '−130', '433.000', 'field beta', 'Ultra'):
    assert phrase in root_readme, f'outdated/missing current onboarding phrase: {phrase}'
    checks += 1
print(f'Public documentation validation passed: {checks} file/link/content checks')
