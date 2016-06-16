#!/usr/bin/env bash

# These files are not included in the diff. Globs beginning with *
# match any path prefix. Without the *, it matches the exact path.
# These are git-specific/submodule files.
ignore_globs="*.gitignore *.gitmodules csi projects/compiler-rt tools/clang"

# Name of the LLVM upstream remote.
upstream_remote="llvm-origin"

# Where to put the patches
patchdir=$(readlink -en `pwd`)

# Project paths
llvm_root=".."
llvm_root=$(readlink -en ${llvm_root})
clang_root="${llvm_root}/tools/clang"
compilerrt_root="${llvm_root}/projects/compiler-rt"

function check_remote {
    git remote -v | grep ${upstream_remote} > /dev/null
    if [[ $? -ne 0 ]]; then
        echo "No upstream LLVM remote ${upstream_remote} in `pwd`."
        exit 1
    fi
}

function check_remotes {
    pushd ${llvm_root} > /dev/null; check_remote; popd > /dev/null
    pushd ${clang_root} > /dev/null; check_remote; popd > /dev/null
    pushd ${compilerrt_root} > /dev/null; check_remote; popd > /dev/null
}

function get_exclude_str {
    for glob in ${ignore_globs}; do
        echo -ne "\":(exclude)${glob}\" "
    done
}

function make_diff {
    projectdir=$1
    project=$(basename $projectdir)
    exclude=$(get_exclude_str)
    patchfile="${patchdir}/${project}.patch"
    pushd $projectdir > /dev/null
    # Eval is necessary to prevent bash from quoting the exclude argument.
    cmd="git diff -U999999 ${upstream_remote}/master...HEAD -- . ${exclude}"
    eval $cmd > $patchfile
    echo "Wrote patch file $patchfile."
    popd > /dev/null
}

check_remotes
make_diff ${llvm_root}
make_diff ${clang_root}
make_diff ${compilerrt_root}
