class IndexRecord:
    key: str
    ptr_value: int
    data_offset: int
    data_size: int
    _idxlen: int

    _PTR_SIZE: int
    _IDXLEN_SIZE: int

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
