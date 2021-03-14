#include <deque>

using namespace std;

deque<int> g_deq;
int g_timeslice;
int current_timeslice;

/**
 * Функция будет вызвана перед каждым тестом, если вы
 * используете глобальные и/или статические переменные
 * не полагайтесь на то, что они заполнены 0 - в них
 * могут храниться значения оставшиеся от предыдущих
 * тестов.
 *
 * timeslice - квант времени, который нужно использовать.
 * Поток смещается с CPU, если пока он занимал CPU функция
 * timer_tick была вызвана timeslice раз.
 **/
void scheduler_setup(int timeslice)
{
	g_deq.clear();
	g_timeslice = timeslice;
	current_timeslice = -1;
}

void next_thread() {
	if (current_timeslice > 0) {
		return;
	}

	if (!g_deq.empty()) {
		current_timeslice = g_timeslice;
	}
	else {
		current_timeslice = -1;
	}
}

/**
 * Функция вызывается, когда создается новый поток управления.
 * thread_id - идентификатор этого потока и гарантируется, что
 * никакие два потока не могут иметь одинаковый идентификатор.
 **/
void new_thread(int thread_id)
{
	g_deq.push_back(thread_id);
	next_thread();
}

/**
 * Функция вызывается, когда поток, исполняющийся на CPU,
 * завершается. Завершится может только поток, который сейчас
 * исполняется, поэтому thread_id не передается. CPU должен
 * быть отдан другому потоку, если есть активный
 * (незаблокированный и незавершившийся) поток.
 **/
void exit_thread()
{
	g_deq.pop_front();
	current_timeslice = -1;
	next_thread();
}

/**
 * Функция вызывается, когда поток, исполняющийся на CPU,
 * блокируется. Заблокироваться может только поток, который
 * сейчас исполняется, поэтому thread_id не передается. CPU
 * должен быть отдан другому активному потоку, если таковой
 * имеется.
 **/
void block_thread()
{
	g_deq.pop_front();
	current_timeslice = -1;
	next_thread();
}

/**
 * Функция вызывается, когда один из заблокированных потоков
 * разблокируется. Гарантируется, что thread_id - идентификатор
 * ранее заблокированного потока.
 **/
void wake_thread(int thread_id)
{
	g_deq.push_back(thread_id);
	next_thread();
}

/**
 * Ваш таймер. Вызывается каждый раз, когда проходит единица
 * времени.
 **/
void timer_tick()
{
	if (current_timeslice > 0) {
		current_timeslice--;
	}

	if (current_timeslice == 0) {
		g_deq.push_back(g_deq.front());
		g_deq.pop_front();
		next_thread();
	}
}

/**
 * Функция должна возвращать идентификатор потока, который в
 * данный момент занимает CPU, или -1 если такого потока нет.
 * Единственная ситуация, когда функция может вернуть -1, это
 * когда нет ни одного активного потока (все созданные потоки
 * либо уже завершены, либо заблокированы).
 **/
int current_thread()
{
	if (g_deq.empty()) {
		return -1;
	}
	else {
		return g_deq.front();
	}
}
