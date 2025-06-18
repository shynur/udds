#! /bin/bash

cd `dirname $0`/..

rm -rf /tmp/shynur-udds-{git,bin}.d
mv .git /tmp/shynur-udds-git.d
mv  bin /tmp/shynur-udds-bin.d

rm -rf  ./* ./.*

mv /tmp/shynur-udds-git.d .git
mv /tmp/shynur-udds-bin.d  bin

git reset --hard HEAD
git pull
