#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ts_library
import ts_definitions
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)


class TsLibraryTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._additional_flags = []

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _build_project1(self):
    gen_dir = os.path.join(self._out_folder, 'project1')

    # Generate definition .d.ts file for legacy JS file.
    ts_definitions.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project1'),
        '--gen_dir',
        gen_dir,
        '--js_files',
        'legacy_file.js',
    ])

    # Build project1, which includes a mix of TS and definition files.
    ts_library.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project1'),
        '--gen_dir',
        gen_dir,
        '--sources',
        'foo.ts',
        '--definitions',
        'legacy_file.d.ts',
    ])
    return gen_dir

  def _assert_project1_output(self, gen_dir):
    os.path.exists(os.path.join(gen_dir, 'foo.d.ts'))
    os.path.exists(os.path.join(gen_dir, 'foo.js'))
    os.path.exists(os.path.join(gen_dir, 'legacy_file.d.ts'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.json'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.manifest'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.tsbuildinfo'))

  # Builds project2 which depends on files from project1, both via relative
  # URLs, as well as via absolute chrome:// URLs.
  def _build_project2(self, project1_gen_dir):
    gen_dir = os.path.join(self._out_folder, 'project2')
    project1_gen_dir = os.path.relpath(project1_gen_dir, gen_dir)

    ts_library.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project2'),
        '--gen_dir',
        gen_dir,
        '--sources',
        'bar.ts',
        '--deps',
        os.path.join(project1_gen_dir, 'tsconfig.json'),
        '--path_mappings',
        'chrome://some-other-source/*|' + os.path.join(project1_gen_dir, '*'),
    ])
    return gen_dir

  def _assert_project2_output(self, gen_dir):
    os.path.exists(os.path.join(gen_dir, 'bar.d.ts'))
    os.path.exists(os.path.join(gen_dir, 'bar.js'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.json'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.manifest'))
    os.path.exists(os.path.join(gen_dir, 'tsconfig.tsbuildinfo'))

  # Test success case where both project1 and project2 are compiled successfully
  # and no errors are thrown.
  def testSuccess(self):
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    project1_gen_dir = self._build_project1()
    self._assert_project1_output(project1_gen_dir)
    project2_gen_dir = self._build_project2(project1_gen_dir)
    self._assert_project2_output(project2_gen_dir)

  # Test error case where a type violation exists, ensure that an error is
  # thrown.
  def testError(self):
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    try:
      ts_library.main([
          '--root_dir',
          os.path.join(_HERE_DIR, 'tests', 'project1'),
          '--gen_dir',
          os.path.join(self._out_folder, 'project1'),
          '--sources',
          'errors.ts',
      ])
    except RuntimeError as err:
      self.assertTrue('Type \'number\' is not assignable to type \'string\'' \
          in err.message)
    else:
      self.fail('Failed to detect type error')


if __name__ == '__main__':
  unittest.main()
