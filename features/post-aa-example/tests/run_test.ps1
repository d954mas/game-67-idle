$ErrorActionPreference = "Stop"
$testSource = Join-Path $PSScriptRoot "test_post_aa_policy.c"
$testBinary = Join-Path $env:TEMP "post-aa-example-policy-test.exe"

& clang -std=c11 -Wall -Wextra -Werror $testSource -o $testBinary
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
& $testBinary
exit $LASTEXITCODE
