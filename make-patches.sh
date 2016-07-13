#!/usr/bin/env bash

# These files are not included in the diff. Globs beginning with *
# match any path prefix. Without the *, it matches the exact path.
# These are git-specific/submodule files.
ignore_globs="*.gitignore *.gitmodules csi projects/compiler-rt tools/clang"

# Name of the LLVM upstream remote.
upstream_remote="llvm-origin"

# Name of base branch.
base_branch="${upstream_remote}/master"

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
    cmd="git diff -U999999 ${base_branch}...HEAD -- . ${exclude}"
    eval $cmd > $patchfile
    echo "Wrote patch file $patchfile."
    popd > /dev/null
}

if [[ $1 == "--help" || $1 == "-h" ]]; then
    echo "Creates diffs suitable for submission to reviews.llvm.org"
    echo ""
    echo "USAGE: $0 [branch]"
    echo ""
    echo "Optional argument [branch] specifies the name of a local branch to diff "
    echo "against. If [branch] is not specified, defaults to ${base_branch}."
    exit 1
fi

if [[ $1 != "" ]]; then
    base_branch=$1
fi

check_remotes
echo Creating diff against ${base_branch}
make_diff ${llvm_root}
make_diff ${clang_root}
make_diff ${compilerrt_root}
