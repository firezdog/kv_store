import db

if __name__ == '__main__':
    DELETE = False
    for i in range(3):
        overwrite = i == 0
        INSERT = i == 0 or DELETE
        DELETE = not DELETE
        print(f'opening database; {overwrite=}, {INSERT=}, {DELETE=}')
        database = db.Database('test', overwrite=overwrite, nhash=1)
        if INSERT:
            print('storing data')
            database.insert('hello', 'world')
            database.insert('hero', 'superman')
            database.insert('greece', 'parthenon')

        results = []

        print('fetching stored data')
        results.append(database.fetch('hello'))
        results.append(database.fetch('hero'))
        results.append(database.fetch('greece'))
        print(results)

        if DELETE:
            print('deleting stored data')
            database.delete('hello')
            database.delete('hero')
            database.delete('greece')
