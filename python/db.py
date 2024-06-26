from io import TextIOWrapper
from os.path import exists
from fcntl import flock, LOCK_EX, LOCK_UN, lockf


class Database:
    _idx_fd: TextIOWrapper
    _dat_fd: TextIOWrapper
    _nhash: int
    _PTR_SIZE = 7       # the size (in chars) of a file offset pointer
    _DB_HASHOFF = 7     # hash table starts after the free list pointer
    _DB_IDX_RECORDS_START: int    # offset for index records portion of index file, used for locking
    _IDXLEN_SIZE = 4

    def __init__(self, name: str, nhash=256, overwrite=False):
        self._nhash = nhash
        # free list size + hash list size + newline
        self._DB_IDX_RECORDS_START = nhash * self._PTR_SIZE + self._DB_HASHOFF + 1

        idx_file = f'{name}.idx'
        dat_file = f'{name}.dat'

        # !! the overwrite flag is dangerous and exists only for testing
        if exists(dat_file) and exists(idx_file) and not overwrite:
            # open without truncating
            self._idx_fd = open(idx_file, 'a+')
            self._dat_fd = open(dat_file, 'a+')
        else:
            # create new file and initialize hash line / free list
            self._idx_fd = open(idx_file, 'w+')
            self._dat_fd = open(dat_file, 'w+')

            # lock while we initialize the first line of the index file -- exclusive as no operations can proceed before init
            flock(self._idx_fd, LOCK_EX)
            # write 1 + nhash hash entries (one more for the free list) and newline
            self._idx_fd.write(''.join([f'{0:>{f"{self._PTR_SIZE}"}}' for i in range(self._nhash + 1)]))
            self._idx_fd.write('\n')
            # unlock
            flock(self._idx_fd, LOCK_UN)

    def insert(self, key: str, value: str):
        # get position of datafile end, then append data to data file
        data_offset = str(self._dat_fd.seek(0, 2))
        data_length = str(self._dat_fd.write(f'{value}\n'))
        # index record is "record_length:key:data_offset:data_length\n"
        index_length = len(key) + 1 + len(data_offset) + 1 + len(data_length) + 1

        # determine where in the hash table the key goes
        key_hash = hash(key) % self._nhash
        hash_offset = key_hash * self._PTR_SIZE + self._DB_HASHOFF

        # read current head of hash chain
        self._idx_fd.seek(hash_offset)
        ptr_value = int(self._idx_fd.read(self._PTR_SIZE))

        # 2. write index record as new head (points to old)
        index_record = f'{ptr_value:>{f"{self._PTR_SIZE}"}}{index_length:>{f"{self._IDXLEN_SIZE}"}}{key}:{data_offset}:{data_length}\n'
        record_offset = f'{self._idx_fd.seek(0, 2):>{f"{self._PTR_SIZE}"}}'
        self._idx_fd.write(index_record)

        # 3. update hash table entry to point to new index record
        self._idx_fd.seek(hash_offset)
        self._idx_fd.write(record_offset)

    def __del__(self):
        self._idx_fd.close()
        self._dat_fd.close()


if __name__ == '__main__':
    db = Database('example', overwrite=True)
    db.insert('a', '1')
    db.insert('b', '2')
    db.insert('c', '3')
