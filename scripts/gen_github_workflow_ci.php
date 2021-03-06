#!/usr/bin/env php
<?php echo "# generated file; do not edit!\n"; ?>

name: ci
on:
  workflow_dispatch:
  push:
  pull_request:

jobs:
<?php

$cur = "8.0";
$gen = include __DIR__ . "/ci/gen-matrix.php";
$job = $gen->github([
"old-matrix" => [
	"PHP" => ["7.0", "7.1", "7.2", "7.3", "7.4"],
	"enable_debug" => "yes",
	"enable_maintainer_zts" => "yes",
	"PECLs" => "propro,pecl_http:http:3.2.4",
], 
"master" => [
	"PHP" => "master",
	"enable_debug" => "yes",
	"enable_zts" => "yes",
	"PECLs" => "m6w6/ext-http:http:master",
],
"cur-dbg-zts" => [
	"PHP" => $cur,
	"enable_debug",
	"enable_zts",
	"PECLs" => "pecl_http:http:4.0.0",
]]);
foreach ($job as $id => $env) {
    printf("  %s:\n", $id);
    printf("    name: %s\n", $id);
    if ($env["PHP"] == "master") {
        printf("    continue-on-error: true\n");
    }
    printf("    env:\n");
    foreach ($env as $key => $val) {
        printf("      %s: \"%s\"\n", $key, $val);
    }
?>
    runs-on: ubuntu-20.04
    steps:
      - uses: actions/checkout@v2
        with:
          submodules: true
      - name: Install
        run: |
          sudo apt-get install -y \
            php-cli \
            php-pear \
            libcurl4-openssl-dev \
            re2c
      - name: Prepare
        run: |
          make -f scripts/ci/Makefile php || make -f scripts/ci/Makefile clean php
      - name: Build
        run: |
          make -f scripts/ci/Makefile ext PECL=raphf
      - name: Prepare Test
        run: |
          if test -n "$PECLs"; then
            IFS=$','
            for pecl in $PECLs; do
              make -f scripts/ci/Makefile pecl PECL=$pecl
            done
            unset IFS
          fi
      - name: Test
        run: |
          make -f scripts/ci/Makefile test
<?php if (isset($env["CFLAGS"]) && strpos($env["CFLAGS"], "--coverage") != false) : ?>
      - name: Coverage
        if: success()
        run: |
          cd src/.libs
          bash <(curl -s https://codecov.io/bash) -X xcode -X coveragepy
<?php endif; ?>

<?php
}
