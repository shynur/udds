#! /bin/bash

cd `dirname $0`/..

mv .git /tmp/shynur-udds-git-dir

rm -rf ./* ./.*

mv /tmp/shynur-udds-git-dir .git

git reset --hard HEAD
