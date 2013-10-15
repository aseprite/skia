#!/usr/bin/python

"""
Copyright 2013 Google Inc.

Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.

Repackage expected/actual GM results as needed by our HTML rebaseline viewer.
"""

# System-level imports
import fnmatch
import json
import logging
import os
import re
import sys
import time

# Imports from within Skia
#
# We need to add the 'gm' directory, so that we can import gm_json.py within
# that directory.  That script allows us to parse the actual-results.json file
# written out by the GM tool.
# Make sure that the 'gm' dir is in the PYTHONPATH, but add it at the *end*
# so any dirs that are already in the PYTHONPATH will be preferred.
GM_DIRECTORY = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
if GM_DIRECTORY not in sys.path:
  sys.path.append(GM_DIRECTORY)
import gm_json

IMAGE_FILENAME_RE = re.compile(gm_json.IMAGE_FILENAME_PATTERN)
CATEGORIES_TO_SUMMARIZE = [
    'builder', 'test', 'config', 'resultType',
]
RESULTS_ALL = 'all'
RESULTS_FAILURES = 'failures'

class Results(object):
  """ Loads actual and expected results from all builders, supplying combined
  reports as requested.

  Once this object has been constructed, the results are immutable.  If you
  want to update the results based on updated JSON file contents, you will
  need to create a new Results object."""

  def __init__(self, actuals_root, expected_root):
    """
    Args:
      actuals_root: root directory containing all actual-results.json files
      expected_root: root directory containing all expected-results.json files
    """
    self._actual_builder_dicts = Results._get_dicts_from_root(actuals_root)
    self._expected_builder_dicts = Results._get_dicts_from_root(expected_root)
    self._combine_actual_and_expected()
    self._timestamp = int(time.time())

  def get_timestamp(self):
    """Return the time at which this object was created, in seconds past epoch
    (UTC).
    """
    return self._timestamp

  def get_results_of_type(self, type):
    """Return results of some/all tests (depending on 'type' parameter).

    Args:
      type: string describing which types of results to include; must be one
            of the RESULTS_* constants

    Results are returned as a dictionary in this form:

       {
         'categories': # dictionary of categories listed in
                       # CATEGORIES_TO_SUMMARIZE, with the number of times
                       # each value appears within its category
         {
           'resultType': # category name
           {
             'failed': 29, # category value and total number found of that value
             'failure-ignored': 948,
             'no-comparison': 4502,
             'succeeded': 38609,
           },
           'builder':
           {
             'Test-Mac10.6-MacMini4.1-GeForce320M-x86-Debug': 1286,
             'Test-Mac10.6-MacMini4.1-GeForce320M-x86-Release': 1134,
             ...
           },
           ... # other categories from CATEGORIES_TO_SUMMARIZE
         }, # end of 'categories' dictionary

         'testData': # list of test results, with a dictionary for each
         [
           {
             'builder': 'Test-Mac10.6-MacMini4.1-GeForce320M-x86-Debug',
             'test': 'bigmatrix',
             'config': '8888',
             'resultType': 'failed',
             'expectedHashType': 'bitmap-64bitMD5',
             'expectedHashDigest': '10894408024079689926',
             'actualHashType': 'bitmap-64bitMD5',
             'actualHashDigest': '2409857384569',
           },
           ...
         ], # end of 'testData' list
       }
    """
    return self._results[type]

  @staticmethod
  def _get_dicts_from_root(root, pattern='*.json'):
    """Read all JSON dictionaries within a directory tree.

    Args:
      root: path to root of directory tree
      pattern: which files to read within root (fnmatch-style pattern)

    Returns:
      A meta-dictionary containing all the JSON dictionaries found within
      the directory tree, keyed by the builder name of each dictionary.

    Raises:
      IOError if root does not refer to an existing directory
    """
    if not os.path.isdir(root):
      raise IOError('no directory found at path %s' % root)
    meta_dict = {}
    for dirpath, dirnames, filenames in os.walk(root):
      for matching_filename in fnmatch.filter(filenames, pattern):
        builder = os.path.basename(dirpath)
        if builder.endswith('-Trybot'):
          continue
        fullpath = os.path.join(dirpath, matching_filename)
        meta_dict[builder] = gm_json.LoadFromFile(fullpath)
    return meta_dict

  def _combine_actual_and_expected(self):
    """Gathers the results of all tests, across all builders (based on the
    contents of self._actual_builder_dicts and self._expected_builder_dicts),
    and stores them in self._results.
    """
    categories_all = {}
    categories_failures = {}
    Results._ensure_included_in_category_dict(categories_all,
                                              'resultType', [
        gm_json.JSONKEY_ACTUALRESULTS_FAILED,
        gm_json.JSONKEY_ACTUALRESULTS_FAILUREIGNORED,
        gm_json.JSONKEY_ACTUALRESULTS_NOCOMPARISON,
        gm_json.JSONKEY_ACTUALRESULTS_SUCCEEDED,
        ])
    Results._ensure_included_in_category_dict(categories_failures,
                                              'resultType', [
        gm_json.JSONKEY_ACTUALRESULTS_FAILED,
        gm_json.JSONKEY_ACTUALRESULTS_FAILUREIGNORED,
        gm_json.JSONKEY_ACTUALRESULTS_NOCOMPARISON,
        ])

    data_all = []
    data_failures = []
    for builder in sorted(self._actual_builder_dicts.keys()):
      actual_results_for_this_builder = (
          self._actual_builder_dicts[builder][gm_json.JSONKEY_ACTUALRESULTS])
      for result_type in sorted(actual_results_for_this_builder.keys()):
        results_of_this_type = actual_results_for_this_builder[result_type]
        if not results_of_this_type:
          continue
        for image_name in sorted(results_of_this_type.keys()):
          actual_image = results_of_this_type[image_name]
          try:
            # TODO(epoger): assumes a single allowed digest per test
            expected_image = (
                self._expected_builder_dicts
                    [builder][gm_json.JSONKEY_EXPECTEDRESULTS]
                    [image_name][gm_json.JSONKEY_EXPECTEDRESULTS_ALLOWEDDIGESTS]
                    [0])
          except (KeyError, TypeError):
            # There are several cases in which we would expect to find
            # no expectations for a given test:
            #
            # 1. result_type == NOCOMPARISON
            #   There are no expectations for this test yet!
            #
            # 2. ignore-tests.txt
            #   If a test has been listed in ignore-tests.txt, then its status
            #   may show as FAILUREIGNORED even if it doesn't have any
            #   expectations yet.
            #
            # 3. alternate rendering mode failures (e.g. serialized)
            #   In cases like
            #   https://code.google.com/p/skia/issues/detail?id=1684
            #   ('tileimagefilter GM test failing in serialized render mode'),
            #   the gm-actuals will list a failure for the alternate
            #   rendering mode even though we don't have explicit expectations
            #   for the test (the implicit expectation is that it must
            #   render the same in all rendering modes).
            #
            # Don't log types 1 or 2, because they are common.
            # Log other types, because they are rare and we should know about
            # them, but don't throw an exception, because we need to keep our
            # tools working in the meanwhile!
            if result_type not in [
                gm_json.JSONKEY_ACTUALRESULTS_NOCOMPARISON,
                gm_json.JSONKEY_ACTUALRESULTS_FAILUREIGNORED] :
              logging.warning('No expectations found for test: %s' % {
                  'builder': builder,
                  'image_name': image_name,
                  'result_type': result_type,
                  })
            expected_image = [None, None]

          # If this test was recently rebaselined, it will remain in
          # the 'failed' set of actuals until all the bots have
          # cycled (although the expectations have indeed been set
          # from the most recent actuals).  Treat these as successes
          # instead of failures.
          #
          # TODO(epoger): Do we need to do something similar in
          # other cases, such as when we have recently marked a test
          # as ignoreFailure but it still shows up in the 'failed'
          # category?  Maybe we should not rely on the result_type
          # categories recorded within the gm_actuals AT ALL, and
          # instead evaluate the result_type ourselves based on what
          # we see in expectations vs actual checksum?
          if expected_image == actual_image:
            updated_result_type = gm_json.JSONKEY_ACTUALRESULTS_SUCCEEDED
          else:
            updated_result_type = result_type

          (test, config) = IMAGE_FILENAME_RE.match(image_name).groups()
          results_for_this_test = {
              'builder': builder,
              'test': test,
              'config': config,
              'resultType': updated_result_type,
              'actualHashType': actual_image[0],
              'actualHashDigest': str(actual_image[1]),
              'expectedHashType': expected_image[0],
              'expectedHashDigest': str(expected_image[1]),
          }
          Results._add_to_category_dict(categories_all, results_for_this_test)
          data_all.append(results_for_this_test)
          if updated_result_type != gm_json.JSONKEY_ACTUALRESULTS_SUCCEEDED:
            Results._add_to_category_dict(categories_failures,
                                       results_for_this_test)
            data_failures.append(results_for_this_test)

    self._results = {
      RESULTS_ALL:
        {'categories': categories_all, 'testData': data_all},
      RESULTS_FAILURES:
        {'categories': categories_failures, 'testData': data_failures},
    }

  @staticmethod
  def _add_to_category_dict(category_dict, test_results):
    """Add test_results to the category dictionary we are building.
    (See documentation of self.get_results_of_type() for the format of this
    dictionary.)

    Args:
      category_dict: category dict-of-dicts to add to; modify this in-place
      test_results: test data with which to update category_list, in a dict:
         {
           'category_name': 'category_value',
           'category_name': 'category_value',
           ...
         }
    """
    for category in CATEGORIES_TO_SUMMARIZE:
      category_value = test_results.get(category)
      if not category_value:
        continue  # test_results did not include this category, keep going
      if not category_dict.get(category):
        category_dict[category] = {}
      if not category_dict[category].get(category_value):
        category_dict[category][category_value] = 0
      category_dict[category][category_value] += 1

  @staticmethod
  def _ensure_included_in_category_dict(category_dict,
                                        category_name, category_values):
    """Ensure that the category name/value pairs are included in category_dict,
    even if there aren't any results with that name/value pair.
    (See documentation of self.get_results_of_type() for the format of this
    dictionary.)

    Args:
      category_dict: category dict-of-dicts to modify
      category_name: category name, as a string
      category_values: list of values we want to make sure are represented
                       for this category
    """
    if not category_dict.get(category_name):
      category_dict[category_name] = {}
    for category_value in category_values:
      if not category_dict[category_name].get(category_value):
        category_dict[category_name][category_value] = 0
