from io import TextIOWrapper
from os.path import exists


from record import IndexRecord
import settings


class Database:
    _idx_fd: TextIOWrapper
    _dat_fd: TextIOWrapper
    _nhash: int
    _DB_IDX_RECORDS_START: int    # offset for index records portion of index file, used for locking

    def __init__(self, name: str, nhash=256, overwrite=False):
        self._nhash = nhash
        # free list size + hash list size + newline
        self._DB_IDX_RECORDS_START = nhash * settings.POINTER_SIZE + settings.HASH_OFFSET + 1

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

            # write 1 + nhash hash entries (one more for the free list) and newline
            self._idx_fd.write(''.join([f'{0:>{f"{settings.POINTER_SIZE}"}}' for i in range(self._nhash + 1)]))
            self._idx_fd.write('\n')

    def delete(self, key):
        # find record to delete
        delendum = self._get_record(key)
        if not delendum.key:
            raise KeyError(f'{key=} does not exist in the database')

        # write the record's next pointer value to the pointer value of the previous record
        self._idx_fd.seek(delendum.prev_ptr_value)
        self._idx_fd.write(f'{delendum.ptr_value:>{f"{settings.POINTER_SIZE}"}}')

    def insert(self, key: str, value: str):
        # TODO: locking
        if not key or not value:
            raise ValueError('cannot store record for empty key / value')

        # note sure if this is a hack, but first check whether the key is present
        record = self._get_record(key)
        if record.key:
            raise KeyError('cannot insert a record that already exists')

        # get position of datafile end, then append data to data file
        data_offset = self._dat_fd.seek(0, 2)
        data_length = self._dat_fd.write(f'{value}\n')

        # determine where in the hash table the key goes
        hash_offset = self._get_hash_offset(key=key)

        # read current head of hash chain
        self._idx_fd.seek(hash_offset)
        ptr_value = int(self._idx_fd.read(settings.POINTER_SIZE))

        # 2. write index record as new head (points to old)
        # we need to determine where the record was inserted first so we can update the hash index below
        record_offset = f'{self._idx_fd.seek(0, 2):>{f"{settings.POINTER_SIZE}"}}'
        index_record = IndexRecord(
            key=key,
            ptr_value=ptr_value,
            data_offset=data_offset,
            data_size=data_length
        )
        self._idx_fd.write(str(index_record))

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
        prev_ptr_value = hash_offset
        self._idx_fd.seek(hash_offset)
        ptr_value = int(self._idx_fd.read(settings.POINTER_SIZE))
        next_ptr_value = 0

        while ptr_value != 0:
            self._idx_fd.seek(ptr_value)
            next_ptr_value = int(self._idx_fd.read(settings.POINTER_SIZE))
            idxlen = int(self._idx_fd.read(settings.IDXLEN_SIZE))
            entry = self._idx_fd.read(idxlen)
            key, data_offset, data_size = entry.strip().split(':')
            if key == target:
                return IndexRecord(
                    ptr_value=next_ptr_value,
                    position=ptr_value,
                    prev_ptr_value=prev_ptr_value,
                    key=key,
                    data_offset=int(data_offset),
                    data_size=int(data_size),
                )

            prev_ptr_value = ptr_value
            ptr_value = next_ptr_value

        # 3b. if the key was not found, return sentinel record
        return IndexRecord(ptr_value=0)

    def _get_hash_offset(self, key: str) -> int:
        # we cannot use hash() b/c it is not deterministic
        result = 0
        for i, c in enumerate(key):
            result += ord(c) * (i + 1)

        key_hash = result % self._nhash
        hash_offset = key_hash * settings.POINTER_SIZE + settings.HASH_OFFSET

        return hash_offset

    def __del__(self):
        self._idx_fd.close()
        self._dat_fd.close()
