import os
import pytest
from unittest import mock

import darshan
from darshan.cli import summary

@pytest.mark.parametrize(
    "argv", [
        [os.path.abspath("./tests/input/noposix.darshan"), "--output=test.html"],
    ]
)
def test_summary(argv):

    with mock.patch("sys.argv", [""] + argv):
        # move outside of /darshan 
        os.chdir("../../../")
        summary.main()
        expected_save_path = os.path.abspath("test.html")
        assert os.path.exists(expected_save_path)
