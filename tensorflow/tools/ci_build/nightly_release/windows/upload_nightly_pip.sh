#!/bin/bash
# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
set -e
set -x

source tensorflow/tools/ci_build/release/common.sh

# Use a virtual environment to get access to the latest pips
python3.9 -m venv venv && source venv/bin/activate

# Install a more recent version of pip and setuptools as the VM's image is too old
python -m pip install --upgrade pip setuptools

# Install a more recent version of twine
python -m pip install --upgrade twine

# Install a more recent version of wheel (needed for renaming)
python -m pip install --upgrade wheel

# Copy and rename to tf_nightly
for f in $(ls "${KOKORO_GFILE_DIR}"/tf_nightly_gpu*dev*cp3*-cp3*-win_amd64.whl); do
  copy_to_new_project_name "${f}" tf_nightly python
done

OVERALL_RETVAL=0
# Upload the built packages to pypi.
# Note: The windows wheels are built by separate jobs, this one just uploads
# them. This is to only have one single upload job, instead of several like we
# do for Ubuntu and MacOS. The benefit is that we only build the Windows wheels
# only once, so we don't wait 6-7 hours again for the process to end. However,
# if the building job fails, this one will attempt to upload the wheels for the
# previous day. Hence, we are checking here that only the proper wheels get
# updated.
TODAY=`date +%Y%m%d`
WHEEL_PATTERN=tf_nightly*dev"${TODAY}"*cp3*-cp3*-win_amd64.whl
for f in $(find "${KOKORO_GFILE_DIR}" -name "${WHEEL_PATTERN}"); do
  test_tf_whl_size $f
  RETVAL=$?

  # Upload the PIP package if whl test passes.
  if [ ${RETVAL} -eq 0 ]; then
    python -m twine upload -r pypi-warehouse "$f"
  else
    echo "Unable to upload package $f. Size check failed."
    OVERALL_RETVAL=1
  fi
done

exit $OVERALL_RETVAL
