import operator as op

n = int(input())
tasks = map(int, input().split())

tasks = enumerate(tasks)
tasks = sorted(tasks, key=op.itemgetter(1))

print(*(task[0] for task in tasks))
