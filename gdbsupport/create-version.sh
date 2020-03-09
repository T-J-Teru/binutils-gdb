#!/bin/sh

# Copyright (C) 1989-2020 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Create version.c from version.in.
# Usage:
#    create-version.sh PATH-TO-GDB-SRCDIR HOST_ALIAS \
#        TARGET_ALIAS OUTPUT-FILE-NAME

srcdir="$1"
host_alias="$2"
target_alias="$3"
output="$4"

GIT_TAG=""
if (cd "$srcdir" && git rev-parse --show-toplevel >/dev/null 2>/dev/null); then

    # The git version string looks something like this:
    #
    #     137acc6fbd6-72fbdf834da-2-dirty
    #     |         | |         | | |   |
    #     '---A-----' '-----B---' C '-D-'
    #
    # Where:
    #   A - This is the last git commit sha.  This represents the HEAD of the
    #       branch that was built.
    #   B - This is the merge base between the current branch and the
    #       upstream master branch.
    #   C - The number of commits from the merge base to the current sha.
    #       This gives an impression of how diverged the branch is.
    #   D - Is the current git tree fully committed? If not then it is dirty.
    #
    # First figure out part A, the short sha for the current head:
    short_head_sha=$(cd "$srcdir" && git rev-parse --short HEAD)

    # Now figure out part D, the dirty tag.  This might be left as the
    # empty string if the repository has no local changes.
    dirty_mark=""
    (cd "$srcdir" && git update-index -q --refresh)
    test -z "$(cd "$srcdir" && git diff-index --name-only HEAD --)" ||
	dirty_mark="-dirty"

    # To calculate parts B and C we need to calculate the merge-base
    # between the current branch and a specific remote branch.  The
    # local file 'remote-repository' contains the remote repository
    # url, and the branch name we are comparing against.  Here we
    # extract that information.
    remote_repo_pattern=$(grep ^pattern: \
			       "$srcdir/../gdbsupport/remote-repository" \
			      | cut -d: -f2-)
    remote_repo_branch=$(grep ^branch: \
			      "$srcdir/../gdbsupport/remote-repository" \
			     | cut -d: -f2-)

    # Use the remote url pattern from the remote-repository file to
    # find the local name for the remote git repository.
    remote_name=$( (cd "$srcdir" && git remote -v) \
	| grep "${remote_repo_pattern}" | grep \(fetch\) \
	| awk -F '\t' '{print $1}')
    branch_info=""
    if [ -n "${remote_name}" ]; then
	# We found the local name for the remote repository, we can
	# now go ahead and figure out parts B and C of the version
	# string.
	remote_branch="${remote_name}/${remote_repo_branch}"
	# Check to see if the remote branch contains the current HEAD.
	# If it does then the user is building directly from (their
	# checkout of) the upstream branch.
	if ! (cd "$srcdir"  && git merge-base \
				   --is-ancestor "${short_head_sha}" \
				   "${remote_branch}"); then
	    # The current head sha is not on the remote tracking
	    # branch.  We need to figure out the merge base, and the
	    # distance from that merge base.
	    merge_base_sha=$(cd "$srcdir" && git merge-base "${short_head_sha}" \
					       "${remote_branch}")
	    short_merge_base_sha=$(cd "$srcdir" \
				       && git rev-parse \
					      --short "${merge_base_sha}")

	    # Count the commits between the merge base and current head.
	    commit_count=$(cd "$srcdir" \
			       && git rev-list --count "${merge_base_sha}"..HEAD)

	    # Now format the parts B and C of the git version string.
	    branch_info="-${short_merge_base_sha}-${commit_count}"
	else
	    # The user is building from a commit on the upstream
	    # branch.  There's no need to include parts B and C of the
	    # git version string.
	    branch_info=""
	fi
    else
	# We couldn't find the local name for the remote repository.
	# Maybe this user has cloned from some other remote, or has
	# deleted their remotes.
	branch_info="-unknown"
    fi
    GIT_TAG="${short_head_sha}${branch_info}${dirty_mark}"
fi

rm -f version.c-tmp "$output" version.tmp
date=$(sed -n -e 's/^.* BFD_VERSION_DATE \(.*\)$/\1/p' "$srcdir/../bfd/version.h")
sed -e "s/DATE/$date/" < "$srcdir/version.in" > version.tmp
if [ -n "$GIT_TAG" ]; then
    echo " ($GIT_TAG)" >> version.tmp
fi
{
    echo '#include "gdbsupport/version.h"'
    echo 'const char version[] = "'"$(sed q version.tmp)"'";'
    echo 'const char host_name[] = "'"$host_alias"'";'
    echo 'const char target_name[] = "'"$target_alias"'";'
} >> version.c-tmp
mv version.c-tmp "$output"
rm -f version.tmp
