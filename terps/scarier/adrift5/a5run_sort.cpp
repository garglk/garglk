/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the .NET introspective sort.
 *
 * clsTask.Children orders a parent's Specific overrides with
 * List(Of String).Sort(TaskPrioritySortComparer), i.e. .NET's UNSTABLE
 * introsort.  Ties on Priority therefore resolve to wherever that particular
 * algorithm happens to leave them, so matching the Adrift 5 runner means porting
 * it byte-for-byte rather than reaching for std::sort or std::stable_sort.  Split
 * out of a5run_action.cpp: it is self-contained (it touches nothing but task
 * priorities) and is the one piece of that file testable in isolation.
 *
 * Entry point: net_introspective_sort, declared in a5run_internal.h.
 */

#include <vector>

#include "a5model.h"
#include "a5run.h"
#include "a5run_internal.h"

/* clsTask.Children sorts a parent's Specific overrides with
   List(Of String).Sort(TaskPrioritySortComparer) (clsTask.vb:328-345) -- .NET's
   UNSTABLE introspective sort, keyed on Priority alone.  When two passing
   overrides tie on priority the one that runs is decided by where introsort
   happens to leave them, so being faithful means porting the algorithm
   byte-for-byte (.NET ArraySortHelper<T>.IntrospectiveSort with a comparer,
   IntrosortSizeThreshold 16; dotnet/runtime ArraySortHelper.cs).  E.g. Fortress
   of Fear has two 'talk to baker' Overrides of Task66 both at priority 1211 --
   Task58 (2023 revision: +3 score, "baker,") and Task1803 (2015: no score,
   "baker, ") -- and the runner runs Task1803 because introsort orders the tie
   [Task2536, Task1803, Task58].  A stable sort keeps XML order and runs Task58:
   wrong text AND a +3 score drift.  The sort input is the parent's children in
   XML load order (TaskHashTable = Dictionary preserves insertion order), with
   completed non-repeatable tasks filtered out BEFORE sorting (the Children
   property's bIncludeCompleted=False filter) -- the filter changes the array
   introsort sees, so it must precede the sort exactly as in the runner. */

static int
net_cmp_pri (const a5_adventure_t *adv, int a, int b)
{
  long pa = adv->tasks[a].priority, pb = adv->tasks[b].priority;
  return pa < pb ? -1 : pa > pb ? 1 : 0;
}

static void
net_swap_if_greater (const a5_adventure_t *adv, int *k, int i, int j)
{
  if (net_cmp_pri (adv, k[i], k[j]) > 0)
    { int t = k[i]; k[i] = k[j]; k[j] = t; }
}

static void
net_insertion_sort (const a5_adventure_t *adv, int *k, int n)
{
  for (int i = 0; i < n - 1; i++)
    {
      int t = k[i + 1];
      int j = i;
      while (j >= 0 && net_cmp_pri (adv, t, k[j]) < 0)
        { k[j + 1] = k[j]; j--; }
      k[j + 1] = t;
    }
}

static void
net_down_heap (const a5_adventure_t *adv, int *k, int i, int n)
{
  int d = k[i - 1];
  while (i <= n / 2)
    {
      int child = 2 * i;
      if (child < n && net_cmp_pri (adv, k[child - 1], k[child]) < 0)
        child++;
      if (!(net_cmp_pri (adv, d, k[child - 1]) < 0))
        break;
      k[i - 1] = k[child - 1];
      i = child;
    }
  k[i - 1] = d;
}

static void
net_heap_sort (const a5_adventure_t *adv, int *k, int n)
{
  for (int i = n / 2; i >= 1; i--)
    net_down_heap (adv, k, i, n);
  for (int i = n; i > 1; i--)
    {
      int t = k[0]; k[0] = k[i - 1]; k[i - 1] = t;
      net_down_heap (adv, k, 1, i - 1);
    }
}

static int
net_pick_pivot_and_partition (const a5_adventure_t *adv, int *k, int n)
{
  int hi = n - 1;
  int middle = hi >> 1;
  net_swap_if_greater (adv, k, 0, middle);
  net_swap_if_greater (adv, k, 0, hi);
  net_swap_if_greater (adv, k, middle, hi);
  int pivot = k[middle];
  { int t = k[middle]; k[middle] = k[hi - 1]; k[hi - 1] = t; }
  int left = 0, right = hi - 1;
  while (left < right)
    {
      while (net_cmp_pri (adv, k[++left], pivot) < 0) ;
      while (net_cmp_pri (adv, pivot, k[--right]) < 0) ;
      if (left >= right)
        break;
      { int t = k[left]; k[left] = k[right]; k[right] = t; }
    }
  if (left != hi - 1)
    { int t = k[left]; k[left] = k[hi - 1]; k[hi - 1] = t; }
  return left;
}

static void
net_intro_sort (const a5_adventure_t *adv, int *k, int n, int depth_limit)
{
  int partition_size = n;
  while (partition_size > 1)
    {
      if (partition_size <= 16)
        {
          if (partition_size == 2)
            { net_swap_if_greater (adv, k, 0, 1); return; }
          if (partition_size == 3)
            {
              net_swap_if_greater (adv, k, 0, 1);
              net_swap_if_greater (adv, k, 0, 2);
              net_swap_if_greater (adv, k, 1, 2);
              return;
            }
          net_insertion_sort (adv, k, partition_size);
          return;
        }
      if (depth_limit == 0)
        { net_heap_sort (adv, k, partition_size); return; }
      depth_limit--;
      int p = net_pick_pivot_and_partition (adv, k, partition_size);
      net_intro_sort (adv, k + p + 1, partition_size - (p + 1), depth_limit);
      partition_size = p;
    }
}

void
net_introspective_sort (const a5_adventure_t *adv, std::vector<int> &keys)
{
  int n = (int) keys.size ();
  if (n <= 1)
    return;
  int log2n = 0;
  for (unsigned u = (unsigned) n; u > 1; u >>= 1)
    log2n++;
  net_intro_sort (adv, &keys[0], n, 2 * (log2n + 1));
}
