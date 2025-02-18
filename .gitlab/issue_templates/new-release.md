<!--
This template is for tracking a release of ParaView. Please replace the
following strings with the associated values:

  - `@VERSION@` - replace with base version, e.g., 5.7.0
  - `@RC@` - for release candidates, replace with "-RC?". For final, replace with "".
  - `@MAJOR@` - replace with major version number
  - `@MINOR@` - replace with minor version number
  - `@PATCH@` - replace with patch version number
  - `@BASEBRANCH@`: The branch to create the release on (for `x.y.0-RC1`,
    `master`, otherwise `release`)
  - `@BRANCHPOINT@`: The ParaView commit where the release should be started

Please remove this comment.
-->

# Preparatory steps

  - macOS signing machine
    - [ ] Check that the macOS signing machine is reachable. If not, request it to be switched on.

# Update ParaView

  - Update the local copy of `@BASEBRANCH@`.
    - If `@PATCH@@RC@` is `0-RC1`, update `master`
    - Otherwise, update `release`
      ```
      git fetch origin
      git checkout @BASEBRANCH@
      git merge --ff-only origin/@BASEBRANCH@ # if this fails, there are local commits that need to be removed
      git submodule update --recursive --init
      ```
    - If `@BASEBRANCH@` is not `master`, ensure merge requests which should be
      in the release have been merged. The [`backport-mrs.py`][backport-mrs]
      script can be used to find and ensure that merge requests assigned to the
      associated milestone are available on the `release` branch.

  - Integrate changes.
    - Make a commit for each of these `release` changes on a single topic
      (suggested branch name: `update-to-v@VERSION@`):
      - [ ] Assemble release notes into `Documentation/release/ParaView-@VERSION@.md`.
      - [ ] Update `version.txt` and tag the commit (tag this commit below)
        ```
        git checkout -b update-to-v@VERSION@@RC@ @BRANCHPOINT@
        echo @VERSION@@RC@ > version.txt
        git commit -m 'Update version number to @VERSION@@RC@' version.txt
        ```
    - Make a commit for each of these `release`-only changes
<!-- if not RC1 and patch == 0 -->
      - [ ] Update VTK's `paraview/release` branch. The
            [`release-mr`][release-mr]  script should be used to do this. Pass
            `-c .kitware-release-paraview.json` to use the appropriate
            configuration file.
        - [ ] In VTK repository make a branch for the current paraview VTK
          ```
          git fetch origin
          git checkout -b update-for-v@VERSION@@RC@ @PARAVIEW_VTK_CHECKSUM@
          ```
        - [ ] Add your development fork as a remote named `gitlab`
          ```
          git remote add gitlab <vtk fork url>
          ```
        - [ ] Obtain a GitLab API token for the `kwrobot.release.paraview` user
              (ask @ben.boeckel if you do not have one)
        - [ ] Add the `kwrobot.release.paraview` user to your fork with at least
              `Developer` privileges (so it can open MRs)
<!-- endif -->
        - [ ] Merge the VTK `paraview/release` update MR
        - [ ] Update kwrobot with the new `paraview/release` branch rules (@ben.boeckel)
      - [ ] `.gitmodules` to track the `paraview/release` branch of VTK
      - [ ] Update `.gitlab/ci/cdash-groups.json` to track the `release` CDash
            groups
    - Create a merge request targeting `release`
      - [ ] Obtain a GitLab API token for the `kwrobot.release.paraview` user
            (ask @ben.boeckel if you do not have one)
      - [ ] Add the `kwrobot.release.paraview` user to your fork with at least
            `Developer` privileges (so it can open MRs)
      - [ ] Use [the `release-mr`][release-mr] script to open the create the
            Merge Request (see script for usage)
        - [ ] Pull the script for each release; it may have been updated since it
          was last used
        - [ ] Add your development fork as a remote named `gitlab`
        - [ ] `release-mr.py -t TOKEN_STRING -c .kitware-release.json -m @BRANCHPOINT@`
        - [ ] The script outputs the information it will be using to create the
          merge request. Please verify that it is all correct before creating
          the merge request. See usage at the top of the script to provide
          information that is either missing or incorrect (e.g., if its data
          extraction heuristics fail).
    - [ ] Run the test with `Do: test` and get a green dashboard.
<!-- if not RC1 and patch == 0 -->
    - [ ] Remove fixup commit: `git reset --hard @^`
<!-- endif -->
    - [ ] Get positive review
    - [ ] `Do: merge`
    - [ ] Create tag: `git tag -a -m '@VERSION@@RC@' v@VERSION@@RC@ commit-that-updated-version.txt`
  - Create tarballs
    - [ ] ParaView (`Utilities/Maintenance/create_tarballs.bash --txz --tgz --zip -v v@VERSION@@RC@`)
  - Upload tarballs to `paraview.org`
    - [ ] Setup your `~/.ssh/config` and add the web host (@vbolea).
    - [ ] `rsync -rptv $tarballs web:ParaView_Release/v@MAJOR@.@MINOR@/`
  - Software process updates (these can all be done independently)
    - [ ] Update kwrobot with the new `release` branch rules (@ben.boeckel)
    - [ ] Run [this script][cdash-update-groups] to update the CDash groups
      - This must be done after a nightly run to ensure all builds are in the
        `release` group
      - See the script itself for usage documentation
    - [ ] Add (or update if `@BASEBRANCH@` is `release`) version selection entry
          in paraview-superbuild

[backport-mrs]: https://gitlab.kitware.com/utils/release-utils/-/blob/master/backport-mrs.py
[release-mr]: https://gitlab.kitware.com/utils/release-utils/-/blob/master/release-mr.py
[cdash-update-groups]: https://gitlab.kitware.com/utils/cdash-utils/-/blob/master/cdash-update-groups.py

# Update ParaView-Superbuild

  - [ ] Update @BASEBRANCH@ branch for **paraview-superbuild**
```
git fetch origin
git checkout @BASEBRANCH@
git merge --ff-only origin/@BASEBRANCH@
git submodule update --recursive --init
```
  - [ ] Create new branch `git checkout -b update-to-v@VERSION@@RC@ @BASEBRANCH@`

  - Integrate changes.
    - Update versions
      - [ ] Guide selections in `versions.cmake` and ensure that the paraview
            http URL source is the _DEFAULT_ source.
      - [ ] `paraview_SOURCE_SELECTION` version in `README.md`
      - [ ] `PARAVIEW_VERSION_DEFAULT` in  CMakeLists.txt
      - [ ] Docker: update default tag strings (in `Scripts/docker/ubuntu/development/Dockerfile`)
        - [ ] ARG PARAVIEW_TAG=v@VERSION@@RC@
        - [ ] ARG SUPERBUILD_TAG=v@VERSION@@RC@
        - [ ] ARG PARAVIEW_VERSION_STRING=paraview-@MAJOR@.@MINOR@
      - [ ] Commit changes
        - [ ] `git add README.md versions.cmake CMakeLists.txt Scripts/docker/ubuntu/development/Dockerfile`
        - [ ] `git commit -m "Update the default version to @VERSION@@RC@"`
    - Make a commit for each of these `release`-only changes
      - [ ] Update `.gitlab/ci/cdash-groups.json` to track the `release` CDash
            groups (if `@BASEBRANCH@` is `master`)
      - [ ] Create a commit which will be tagged:
        - [ ] `git commit --allow-empty -m "paraview: add release @VERSION@"`
        - [ ] Create tag: `git tag -a -m 'ParaView superbuild @VERSION@@RC@' v@VERSION@@RC@ HEAD`
<!-- if not RC and patch == 0 -->
      - [ ] Create a commit that changes the paraview _DEFAULT_ source to the git
            url source in the `versions.cmake` file.
<!-- endif -->
      - [ ] Force `@VERSION@@RC@` in CMakeLists.txt
        - [ ] Append to the top of CMakeLists.txt (After project...) The following
            ```
            # Force source selection setting here.
            set(paraview_SOURCE_SELECTION "@VERSION@@RC@" CACHE STRING "Force version to @VERSION@@RC@" FORCE)
            set(paraview_FROM_SOURCE_DIR OFF CACHE BOOL "Force source dir off" FORCE)
            ```
      - [ ] Create fixup commit with the above changes `git commit -a --fixup=@`. The fixup commit will prevent merging of the temporary code above; it will be removed in a future step.
      - [ ] Create a merge request targeting `release`
    - [ ] Obtain a GitLab API token for the `kwrobot.release.paraview` user
          (ask @ben.boeckel if you do not have one)
    - [ ] Add the `kwrobot.release.paraview` user to your fork with at least
          `Developer` privileges (so it can open MRs)
    - [ ] Use [the `release-mr`][release-mr] script to open the create the
          Merge Request (see script for usage)
      - [ ] Pull the script for each release; it may have been updated since it
        was last used
      - [ ] `release-mr.py -t TOKEN_STRING -c .kitware-release.json -m @BRANCHPOINT@`
<!-- if not RC and patch == 0-->
      - [ ] Make sure that the backporting directive in the merge-request
            description skips the last commit such as: `Backport: master:HEAD~`
<!-- endif -->
  - Build binaries
    - [ ] Build binaries (start all pipelines)
    - [ ] Download the binaries that have been generated from the Pipeline
          build products. They will be deleted within 24 hours.

  - [ ] Remove fixup commit: `git reset --hard @^`
  - [ ] Get positive review
  - [ ] Force push `git push -f gitlab`
  - [ ] `Do: merge`
  - Software process updates (these can all be done independently)
    - [ ] Update kwrobot with the new `release` branch rules (@ben.boeckel)
    - [ ] Run [this script][cdash-update-groups] to update the CDash groups
      - This must be done after a nightly run to ensure all builds are in the
        `release` group
      - See the script itself for usage documentation
    - [ ] Add (or update if `@BASEBRANCH@` is `release`) version selection entry
          in paraview-superbuild

# Sign Windows binaries
  -  [ ] Request Windows binary signings (only .exe archives) on the Package
         Signing repo. Example request [here][win-sign-example].

# Sign macOS binaries
  - [ ] Upload to signing server, run script, download resulting .pkg and .dmg files
  - [ ] Install from .pkg and verify that it is signed with `codesign -dvvv /Applications/ParaView-@VERSION@@RC@.app/`
  - [ ] Install from .dmg and verify that it is signed with `codesign -dvvv /Applications/ParaView-@VERSION@@RC@.app/`

# Validating binaries

For each binary, open the Python shell and run the following:

```python
import numpy
s = Show(Sphere())
ColorBy(s, ('POINTS', 'Normals', 'X'))
Show(Text(Text="$A^2$"))
```

  Check that
  - Getting started guide opens
  - Help lists Readers, Writers, Filters, and Sources properly
  - Examples load and match thumbnails in dialog
  - Plugins are present and load properly
  - OSPRay raycasting and pathtracing runs ("Enable Ray Tracing" property in View panel)
  - OptiX pathtracing runs (not macOS)
    - ref. !22372 for current expected results
  - IndeX runs (load pvNVIDIAIndeX plugin, add a Wavelet dataset, change representaiton to NVIDIA IndeX)

Binary checklist
  - [ ] macOS arm64
  - [ ] macOS x86\_64
  - [ ] Linux
  - [ ] Linux osmesa
  - [ ] Windows MPI (.exe)
  - [ ] Windows MPI (.zip)
  - [ ] Windows no-MPI (.exe)
  - [ ] Windows no-MPI (.zip)

# Upload binaries

  - [ ] Upload binaries to `paraview.org` (`rsync -rptv $binaries paraview.release:ParaView_Release/v@MAJOR@.@MINOR@/`)
  - [ ] Ask @cory.quammen to regenerate `https://www.paraview.org/files/listing.txt` and `md5sum.txt` on the website from within the directory corresponding to www.paraview.org/files/

```
buildListing.sh
updateMD5sum.sh v@MAJOR@.@MINOR@
```

  - [ ] Test download links on https://www.paraview.org/download

# Push tags

 - [ ] In the `paraview` repository, run `git push origin v@VERSION@@RC@`.
 - [ ] In the `paraview-superbuild` repository, run `git push origin v@VERSION@@RC@`.

# Spack

 - [ ] Update Spack package: https://github.com/spack/spack/blob/develop/var/spack/repos/builtin/packages/paraview/package.py

<!--
If making a non-RC release:

# Update documentation
  - [ ] Submit a Merge Request for release that updates the version to @VERSION@ in https://gitlab.kitware.com/paraview/paraview-docs/-/blob/master/doc/source/conf.py` for `paraview-docs`
  - [ ] Upload versioned documentation to `https://github.com/kitware/paraview-docs` (see `https://github.com/Kitware/paraview-docs/blob/master/README.md`)
  - [ ] Tag the HEAD of release in [ParaView docs](https://gitlab.kitware.com/paraview/paraview-docs/-/tags) with v@VERSION@.
  - [ ] Activate the tag on [readthedocs](https://readthedocs.org/projects/paraview/versions/) and build it [here](https://readthedocs.org/projects/paraview/)
  - [ ] Go to readthedocs.org and activate
  - [ ] Write and publish blog post with release notes.
  - [ ] Update release notes
    (https://www.paraview.org/Wiki/ParaView_Release_Notes)
-->

# Post-release

  - [ ] Post an announcement in the Announcements category on
        [discourse.paraview.org](https://discourse.paraview.org/).
  - [ ] Request DoD vulnerability scan
  - [ ] Request an XRInterface plugin validation using [TESTING.md](https://gitlab.kitware.com/paraview/paraview/-/blob/master/Plugins/XRInterface/TESTING.md) protocol from KEU

<!--
If making a non-RC release:

  - [ ] Request from comm@kitware.com an update of version number in "Download Latest Release" text on www.paraview.org
  - [ ] Move unclosed issues in GitLab to the next release milestone in GitLab
-->

/cc @ben.boeckel

/cc @cory.quammen

/cc @charles.gueunet

/cc @mwestphal

/label ~"priority:required"

[win-sign-example]:  https://kwgitlab.kitware.com/software-process/package-signing/-/issues/12
