from io import TextIOWrapper
from os.path import exists
from fcntl import flock, LOCK_EX, LOCK_UN, lockf


class Database:
    _idx_fd: TextIOWrapper
    _dat_fd: TextIOWrapper
    _nhash: int
    _PTR_SIZE = 7       # the size (in chars) of a file offset pointer
    _DB_HASHOFF = 7     # hash table starts after the free list pointer
    _DB_IDX_RECORD_START: int    # offset for index records portion of index file, used for locking

    def __init__(self, name: str, nhash=256, overwrite=False):
        self._nhash = nhash
        # free list size + hash list size + newline
        self._DB_IDX_RECORD_START = nhash * self._PTR_SIZE + self._DB_HASHOFF + 1

        idx_file = f'{name}.idx'
        dat_file = f'{name}.dat'

        # !! the overwrite flag is dangerous and exists only for testing
        if exists(dat_file) and exists(idx_file) and not overwrite:
            # open without truncating
            self._idx_fd = open(idx_file, 'a+')
            self._dat_fd = open(dat_file, 'a+')
        else:
            # create new file and initialize hash line / free list
            self._idx_fd = open(idx_file, 'w')
            self._dat_fd = open(dat_file, 'w')

            # lock while we initialize the first line of the index file -- exclusive as no operations can proceed before init
            flock(self._idx_fd, LOCK_EX)
            # write 1 + nhash hash entries (one more for the free list) and newline
            self._idx_fd.write(''.join([f'{0:>{f"{self._PTR_SIZE}"}}' for i in range(self._nhash + 1)]))
            self._idx_fd.write('\n')
            # unlock
            flock(self._idx_fd, LOCK_UN)

    def insert(self, key: str, value: str):
        # get position of datafile end, then append data to data file (w/ exclusive lock)
        flock(self._dat_fd, LOCK_EX)
        data_file_end = self._dat_fd.seek(0, 2)
        written = self._dat_fd.write(f'{value}\n')
        flock(self._dat_fd, LOCK_UN)

        # determine where in the hash table the key goes
        key_hash = hash(key) % self._nhash
        hash_offset = key_hash * self._PTR_SIZE + self._DB_HASHOFF

        # 1. write-lock hash table entry for this key -- read current head of hash chain
        # 2. write index record with write-lock of records portion of file as new head (points to old)
        # 3. update hash table entry to point to new index record
        # 4. un-write-lock hash table entry for this key

    def __del__(self):
        self._idx_fd.close()
        self._dat_fd.close()


if __name__ == '__main__':
    db = Database('example', overwrite=True)
    db.insert('a', '1')
    db.insert('b', '2')
    db.insert('c', '3')
