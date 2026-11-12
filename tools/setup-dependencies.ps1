$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencies = @(
    @{ Name = 'M5Unified'; Revision = '96576a14247ddc935149145aa6b51943464120f1' },
    @{ Name = 'M5GFX'; Revision = '28d47949bdc84f30881623476f555398e2194313'; Owner = 'm5stack' },
    @{ Name = 'esp-usb'; Revision = '18db0b7252eb8d599cec3db2cb9330d946fd4de7'; Owner = 'espressif' }
)
foreach ($dependency in $dependencies) {
    $destination = Join-Path $projectRoot ('components/' + $dependency.Name)
    if (-not (Test-Path -LiteralPath $destination)) {
        # Fetch the verified revision, rather than depending on a moving master branch.
        $owner = if ($dependency.Owner) { $dependency.Owner } else { 'm5stack' }
        git clone ('https://github.com/' + $owner + '/' + $dependency.Name + '.git') $destination
        if ($LASTEXITCODE -ne 0) { throw "Could not fetch $($dependency.Name)" }
        git -C $destination checkout --detach $dependency.Revision
        if ($LASTEXITCODE -ne 0) { throw "Could not check out $($dependency.Name)" }
    }
    $actual = git -C $destination rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $dependency.Revision) {
        throw "$($dependency.Name) has an unexpected revision; expected $($dependency.Revision)."
    }
    Write-Host "$($dependency.Name): $actual"
}
