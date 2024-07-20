import unittest
from unittest import mock
from io import StringIO

from db import Database


@mock.patch('builtins.open', side_effect=lambda *args, **kwargs: StringIO())
@mock.patch('db.exists', return_value=False)
class TestDB(unittest.TestCase):
    def test_insert_and_fetch_without_hash_chaining(self, *args, **kwargs):
        db = Database('test')
        db.insert('hello', 'world')
        db.insert('1', '2')
        self.assertEqual(db.fetch('hello'), 'world')
        self.assertEqual(db.fetch('1'), '2')

    @mock.patch.object(Database, '_get_hash_offset', return_value=42)
    def test_insert_and_fetch_with_hash_chaining(self, *args, **kwargs):
        db = Database('test')
        db.insert('hello', 'world')
        db.insert('1', '2')
        self.assertEqual(db.fetch('hello'), 'world')
        self.assertEqual(db.fetch('1'), '2')

    def test_insert_existing_raises_error(self, *args, **kwargs):
        db = Database('test')
        db.insert('hello', 'world')
        with self.assertRaises(KeyError):
            db.insert('hello', 'world')

    def test_insert_no_key_raises_error(self, *args, **kwargs):
        db = Database('test')
        with self.assertRaises(ValueError):
            db.insert('', 'value')

    def test_insert_no_value_raises_error(self, *args, **kwargs):
        db = Database('test')
        with self.assertRaises(ValueError):
            db.insert('key', '')

    @mock.patch.object(Database, '_get_hash_offset', return_value=42)
    def test_delete(self, *args, **kwargs):
        db = Database('test')
        db.insert('hello', 'world')
        db.insert('goodbye', 'world')
        db.delete('hello')
        db.delete('goodbye')

        with self.assertRaises(KeyError):
            db.fetch('hello')
        with self.assertRaises(KeyError):
            db.fetch('goodbye')

        db.insert('hello', 'doge')
        self.assertEqual(db.fetch('hello'), 'doge')
