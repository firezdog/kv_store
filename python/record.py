import settings


class IndexRecord:
    prev_ptr_value: int
    key: str
    ptr_value: int
    data_offset: int
    data_size: int

    _record: str

    def __init__(self, ptr_value: int, prev_ptr_value=0, key='', data_offset=0, data_size=0):
        self.prev_ptr_value = prev_ptr_value
        self.ptr_value = ptr_value
        self.key = key
        self.data_offset = data_offset
        self.data_size = data_size

        main_record = f'{self.key}:{self.data_offset}:{self.data_size}\n'
        idxlen = len(main_record)
        self._contents = f'{self.ptr_value:>{f"{settings.POINTER_SIZE}"}}{idxlen:>{f"{settings.IDXLEN_SIZE}"}}{main_record}'

    def __repr__(self) -> str:
        return self._contents
