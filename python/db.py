from io import TextIOWrapper
from os.path import exists
from fcntl import flock, LOCK_EX, LOCK_UN


class DuplicateKeyError(Exception):
    ...


class IndexRecord:
    key: str
    ptr_value: int
    data_offset: int
    data_size: int
    _idxlen: int
    _PTR_SIZE: int

    def __init__(self, key: str, ptr_value: int, data_offset: int, data_size: int, ptr_size: int, idxlen_size: int):
        self.key = key
        self.ptr_value = ptr_value
        self.data_offset = data_offset
        self.data_size = data_size

        self._PTR_SIZE = ptr_size
        self._IDXLEN_SIZE = idxlen_size

        # index record is "record_length:key:data_offset:data_length\n"
        self._idxlen = len(key) + 1 + len(str(data_offset)) + 1 + len(str(data_size)) + 1

    @classmethod
    def from_raw(cls, ptr_value: int, raw: str, ptr_size, idxlen_size):
        key, offset, size = raw.strip().split(':')
        return cls(
            ptr_value=ptr_value,
            key=key,
            data_offset=int(offset),
            data_size=int(size),
            ptr_size=ptr_size,
            idxlen_size=idxlen_size,
        )

    def to_raw(self):
        return f'{self.ptr_value:>{f"{self._PTR_SIZE}"}}{self._idxlen:>{f"{self._IDXLEN_SIZE}"}}{self.key}:{self.data_offset}:{self.data_size}\n'

    @classmethod
    def from_hash_entry(cls, ptr_value: int, ptr_size, idxlen_size):
        return cls(
            key='',
            ptr_value=ptr_value,
            data_offset=0,
            data_size=0,
            ptr_size=ptr_size,
            idxlen_size=idxlen_size,
        )

    @classmethod
    def null(cls):
        return cls(
            key='',
            ptr_value=0,
            data_offset=0,
            data_size=0,
            ptr_size=0,
            idxlen_size=0,
        )


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
            self._idx_fd = open(idx_file, 'r+')
            self._dat_fd = open(dat_file, 'r+')
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
        # TODO: locking
        if not key:
            raise ValueError('cannot store record for empty key')

        # note sure if this is a hack, but first check whether the key is present
        record = self._get_record(key)
        if record.key:
            raise IndexError('cannot insert a record that already exists')

        # get position of datafile end, then append data to data file
        data_offset = str(self._dat_fd.seek(0, 2))
        data_length = str(self._dat_fd.write(f'{value}\n'))

        # determine where in the hash table the key goes
        hash_offset = self._get_hash_offset(key=key)

        # read current head of hash chain
        self._idx_fd.seek(hash_offset)
        ptr_value = int(self._idx_fd.read(self._PTR_SIZE))

        # 2. write index record as new head (points to old)
        # we need to determine where the record was inserted first so we can update the hash index below
        record_offset = f'{self._idx_fd.seek(0, 2):>{f"{self._PTR_SIZE}"}}'
        index_record = IndexRecord(
            key=key,
            ptr_value=ptr_value,
            data_offset=data_offset,
            data_size=data_length,
            ptr_size=self._PTR_SIZE,
            idxlen_size=self._IDXLEN_SIZE,
        )
        self._idx_fd.write(index_record.to_raw())

        # 3. update hash table entry to point to new index record
        self._idx_fd.seek(hash_offset)
        self._idx_fd.write(record_offset)

    def fetch(self, key: str) -> str:
        record = self._get_record(target=key)
        if not record.key:
            raise KeyError('key not in database')

        self._dat_fd.seek(record.data_offset)
        data = self._dat_fd.read(record.data_size).strip()
        return data

    def _get_record(self, target: str) -> IndexRecord:
        # TODO: locking
        # 1. determine the hash index offset for the key
        hash_offset = self._get_hash_offset(target)

        # 2. read through each entry in the hash chain until the key is found
        self._idx_fd.seek(hash_offset)
        ptr_value = int(self._idx_fd.read(self._PTR_SIZE))

        record = IndexRecord.from_hash_entry(ptr_value=ptr_value, ptr_size=self._PTR_SIZE, idxlen_size=self._IDXLEN_SIZE)
        while record.ptr_value != 0:
            self._idx_fd.seek(record.ptr_value)
            next_ptr_value = int(self._idx_fd.read(self._PTR_SIZE))
            idxlen = int(self._idx_fd.read(self._IDXLEN_SIZE))
            raw_record = self._idx_fd.read(idxlen)
            record = IndexRecord.from_raw(ptr_value=next_ptr_value, raw=raw_record, ptr_size=self._PTR_SIZE, idxlen_size=self._IDXLEN_SIZE)
            if record.key == target:
                return record

            ptr_value = next_ptr_value

        # 3b. if the key was not found, raise an error
        return IndexRecord.null()

    def _get_hash_offset(self, key: str) -> int:
        # we cannot use hash() b/c it is not deterministic
        result = 0
        for i, c in enumerate(key):
            result += ord(c) * (i + 1)

        key_hash = result % self._nhash
        hash_offset = key_hash * self._PTR_SIZE + self._DB_HASHOFF

        return hash_offset

    def __del__(self):
        self._idx_fd.close()
        self._dat_fd.close()


if __name__ == '__main__':
    overwrite = False
    db = Database('example', overwrite=overwrite)
    if overwrite:
        db.insert('a', 'Anniston')
        db.insert('b', 'Bob')
        db.insert('c', 'Cary')

    print(f'a:{db.fetch('a')}')
    print(f'b:{db.fetch('b')}')
    print(f'c:{db.fetch('c')}')
    try:
        print(f'd:{db.fetch('d')}')
    except KeyError:
        print('did not find "d" in database -- will insert and try again')
        db.insert('d', 'now here')
        print(f'd:{db.fetch('d')}')
