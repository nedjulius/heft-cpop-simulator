#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <set>
#include <float.h>
#include <math.h>
#include <algorithm>
#include <climits>

// command line arguments:
//
// ./simulate A config
// A: 1 - HEFT; 2 - CPOP
// config: configuration file path
//
// configuration format:
//
// read the user input first
// the input is the following:
// - first number is V task count (vertex in DAG count)
// - second number is E edge count (all directed edges in DAG)
// - third number is P processor count
// - the following E lines are: <from> <to> <weight>
//   - these E lines stand for the edges in the graph and the data size between them (in bytes)
// - the following V lines are: <p_1> ... <p_P> (from 1 to P)
//   - p_i stands for computation cost of task j on processor i
// - the following (p^2 - p) / 2 lines stand for: <from> <to> <weight>
//   - each line describes the data transfer rate between processor from and to with weight being bytes per second

class graph {
  private:
    int task_count;
    std::vector<std::vector<int>> adjacency_list;
    std::vector<std::vector<int>> predecessor_list;
  public:
    graph(int task_count);
    void add_edge(int a, int b);
    std::vector<int> get_successors_of(int i);
    std::vector<int> get_predecessors_of(int i);
};

graph::graph(int task_count) {
  this->task_count = task_count;
  for (int i = 0; i < task_count; i++) {
    std::vector<int> edge_list;
    std::vector<int> p_edge_list;
    this->adjacency_list.push_back(edge_list);
    this->predecessor_list.push_back(p_edge_list);
  }
}

void graph::add_edge(int a, int b) {
  this->adjacency_list.at(a).push_back(b);
  this->predecessor_list.at(b).push_back(a);
}

std::vector<int> graph::get_successors_of(int i) {
  return adjacency_list.at(i);
}

std::vector<int> graph::get_predecessors_of(int i) {
  return predecessor_list.at(i);
}

class matrix {
  private:
    int row_length;
    int col_length;
    int* arr;
    size_t get_index(int i, int j);
  public:
    matrix(int row_length, int col_length);
    ~matrix();
    int get(int i, int j);
    void put(int i, int j, int val);
};

matrix::matrix(int row_length, int col_length) {
  this->row_length = row_length;
  this->col_length = col_length;
  size_t arr_size = row_length * col_length;
  arr = new int[arr_size];
  for (int i = 0; i < arr_size; i++)
    arr[i] = 0;
}

matrix::~matrix() {
  delete[] arr;
}

size_t matrix::get_index(int i, int j) {
  return i + row_length * j;
}

int matrix::get(int i, int j) {
  return arr[get_index(i, j)];
}

void matrix::put(int i, int j, int val) {
  arr[get_index(i, j)] = val;
}

// hc_env struct
class hc_env {
  public:
    static hc_env* init_env_from_config(char *config_file_path);
    int processor_count;
    int task_count;
    graph* dag;
    matrix* data;
    matrix* transfer_rates;
    matrix* execution_costs;
    hc_env(int processor_count, int task_count, graph* dag, matrix* data, matrix* transfer_rates, matrix* execution_costs);
    ~hc_env();
};

hc_env::hc_env(int processor_count, int task_count, graph* dag, matrix* data, matrix* transfer_rates, matrix* execution_costs) {
  this->processor_count = processor_count;
  this->task_count = task_count;
  this->dag = dag;
  this->data = data;
  this->transfer_rates = transfer_rates;
  this->execution_costs = execution_costs;
}

hc_env::~hc_env() {
  delete dag;
  delete data;
  delete transfer_rates;
  delete execution_costs;
}

hc_env* hc_env::init_env_from_config(char *config_file_path) {
  std::ifstream config_file(config_file_path);

  if (!config_file.is_open()) {
    std::cout << "Error! Couldn't open the file." << std::endl;
    exit(1);
  }

  int task_count, edge_count, processor_count = 0;
  config_file >> task_count >> edge_count >> processor_count;
  graph* dag = new graph(task_count);
  matrix* data = new matrix(task_count, task_count);
  matrix* execution_costs = new matrix(task_count, processor_count);
  matrix* transfer_rates = new matrix(processor_count, processor_count);
  int a, b, c, i, j;

  for (i = 0; i < edge_count; i++) {
    config_file >> a >> b >> c;
    a--;
    b--;
    dag->add_edge(a, b);
    data->put(a, b, c);
    data->put(b, a, c);
  }

  for (i = 0; i < task_count; i++) {
    for (j = 0; j < processor_count; j++) {
      config_file >> a;
      execution_costs->put(i, j, a);
    }
  }

  int n = (processor_count * processor_count - processor_count) / 2;

  for (i = 0; i < n; i++) {
    config_file >> a >> b >> c;
    a--;
    b--;
    transfer_rates->put(a, b, c);
    transfer_rates->put(b, a, c);
  }

  config_file.close();

  return new hc_env(processor_count, task_count, dag, data, transfer_rates, execution_costs);
}

enum AlgorithmId {
  heft = 1,
  cpop = 2
};

// rank utilities
namespace rank {
  double get_communication_cost(hc_env *hc_env, int i, int j, int p1, int p2) {
    if (p1 == p2) {
      return 0.0;
    }

    return (double)hc_env->data->get(i, j) / (double)hc_env->transfer_rates->get(p1, p2);
  }

  double get_avg_communication_cost(hc_env *hc_env, int i, int j) {
    double avg_transfer_rate = 0.0;

    for (int p_j = 0; p_j < hc_env->processor_count - 1; p_j++)
      avg_transfer_rate += (double)hc_env->transfer_rates->get(p_j, p_j + 1);
    avg_transfer_rate /= (double)(hc_env->processor_count - 1);

    return (double)hc_env->data->get(i, j) / avg_transfer_rate;
  };

  double get_avg_execution_cost(hc_env *hc_env, int i) {
    double w_sum = 0.0;

    for (int p_j = 0; p_j < hc_env->processor_count; p_j++)
      w_sum += (double)hc_env->execution_costs->get(i, p_j);

    return w_sum / hc_env->processor_count;
  };

  double find_upward(hc_env *hc_env, int i) {
    double w = get_avg_execution_cost(hc_env, i);

    // if exit task, return the average execution cost
    if (i == hc_env->task_count - 1)
      return w;

    std::vector<int> succ_i = hc_env->dag->get_successors_of(i);
    double max = 0.0;

    for (int n = 0; n < succ_i.size(); n++) {
      int j = succ_i.at(n);
      max = std::max(max, get_avg_communication_cost(hc_env, i, j) + find_upward(hc_env, j));
    }

    return w + max;
  };

  double find_downward(hc_env *hc_env, int i) {
    // if entry task, return rank 0
    if (i == 0)
      return 0.0;

    std::vector<int> pred_i = hc_env->dag->get_predecessors_of(i);
    double max = 0.0;

    for (int n = 0; n < pred_i.size(); n++) {
      int j = pred_i.at(n);
      double w = get_avg_execution_cost(hc_env, j);
      double c = get_avg_communication_cost(hc_env, j, i);
      max = std::max(max, w + c + find_downward(hc_env, j));
    }

    return max;
  }
}

void run_heft(hc_env *hc_env, std::string write_file_path);
void run_cpop(hc_env *hc_env, std::string write_file_path);

std::string get_write_file_path(char *input_path, int algorithm_id) {
  std::string output_path = input_path;
  std::string ext = ".out";
  std::string alg = algorithm_id == heft ? "--heft" : "--cpop";
  output_path = output_path.substr(0, output_path.size() - 3) + alg + ext;
  return output_path;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: ./<executable> A /path-to-input.in" << std::endl;
    std::cout << "A: 1 - HEFT, 2 - CPOP" << std::endl;
    std::cout << "Refer to README on how to format input" << std::endl;
    return 0;
  }

  int algorithm_id = atoi(argv[1]);
  hc_env *hc_env = hc_env::init_env_from_config(argv[2]);

  switch (algorithm_id) {
    case heft:
      run_heft(hc_env, get_write_file_path(argv[2], algorithm_id));
      break;
    case cpop:
      run_cpop(hc_env, get_write_file_path(argv[2], algorithm_id));
    default:
      break;
  }

  delete hc_env;

  return 0;
}

struct task {
  double rank;
  int node;
  bool operator > (const task& other_task) const {
    return (rank > other_task.rank);
  }
  bool operator < (const task& other_task) const {
    return (rank < other_task.rank);
  }
};

struct process {
  int task_node_id;
  double start_time;
  double end_time;
  int processor_id;
};

std::queue<task> get_sorted_task_queue(std::vector<task> list) {
  std::sort(list.begin(), list.end(), std::greater<task>());
  std::queue<task> q;

  for (int i = 0; i < list.size(); i++)
    q.push(list.at(i));

  return q;
}

std::vector<task> compute_upward_ranks(hc_env *hc_env) {
  int task_count = hc_env->task_count;
  std::vector<task> list(task_count);

  for (int i = task_count - 1; i >= 0; i--) {
    list.at(i).rank = rank::find_upward(hc_env, i);
    list.at(i).node = i;
  }

  return list;
}

std::vector<task> compute_downward_ranks(hc_env *hc_env) {
  int task_count = hc_env->task_count;
  std::vector<task> list(task_count);

  for (int i = 0; i < task_count; i++) {
    list.at(i).rank = rank::find_downward(hc_env, i);
    list.at(i).node = i;
  }

  return list;
}

std::vector<task> compute_priority(hc_env *hc_env) {
  int task_count = hc_env->task_count;
  std::vector<task> priority_list(task_count);
  std::vector<task> downward_rank_list = compute_downward_ranks(hc_env);
  std::vector<task> upward_rank_list = compute_upward_ranks(hc_env);

  for (int i = 0; i < task_count; i++) {
    priority_list.at(i).rank = downward_rank_list.at(i).rank + upward_rank_list.at(i).rank;
    priority_list.at(i).node = i;
  }

  return priority_list;
}

int find_pcp(hc_env *hc_env, std::set<int> set) {
  int processor_count = hc_env->processor_count;
  int min_execution_sum = INT_MAX;
  int min_pcp = -1;

  for (int j = 0; j < processor_count; j++) {
    double curr_sum = 0.0;
    for (const int &node_id : set) {
      curr_sum += hc_env->execution_costs->get(node_id, j);
    }

    if (curr_sum < min_execution_sum) {
      min_execution_sum = curr_sum;
      min_pcp = j;
    }
  }

  return min_pcp;
}

double est(hc_env *hc_env, double *avail, process *scheduled, int i, int p) {
  if (i == 0) {
    return 0;
  }

  double max = 0;
  std::vector<int> pred_i = hc_env->dag->get_predecessors_of(i);

  for (int n = 0; n < pred_i.size(); n++) {
    int j = pred_i.at(n);

    // skip if not scheduled
    if (scheduled[j].task_node_id == -1)
      continue;

    double aft = scheduled[j].end_time;
    double transfer_time = rank::get_communication_cost(hc_env, j, i, scheduled[j].processor_id, p);
    max = std::max(max, aft + transfer_time);
  }

  return std::max(max, avail[p]);
}

double eft(hc_env *hc_env, double *avail, process *scheduled, int i, int p) {
  return (double)hc_env->execution_costs->get(i, p) + est(hc_env, avail, scheduled, i, p);
}

bool is_task_ready(hc_env *hc_env, int i, process *scheduled) {
  std::vector<int> pred_i = hc_env->dag->get_predecessors_of(i);
  bool is_ready = true;

  for (int n = 0; n < pred_i.size(); n++) {
    if (scheduled[pred_i.at(n)].processor_id == -1)
      is_ready = false;
  }

  return is_ready;
}

void write_results(process *scheduled, int task_count, int processor_count, std::string write_file_path) {
  std::ofstream wf(write_file_path);

  if (!wf.is_open()) {
    std::cout << "An unexpected error occurred while trying to open write file." << std::endl;
    exit(1);
  }

  double max_end_time = -1;
  std::vector<int> scheduled_processor_counts(processor_count, 0);

  for (int i = 0; i < task_count; i++) {
    wf << "--- task " << scheduled[i].task_node_id + 1 << " ---\n";
    wf << "Start time: " << scheduled[i].start_time << "\n";
    wf << "Finish time: " << scheduled[i].end_time << "\n";
    wf << "Processor: " << scheduled[i].processor_id + 1 << "\n";
    wf << "\n";
    scheduled_processor_counts.at(scheduled[i].processor_id)++;
    max_end_time = std::max(max_end_time, scheduled[i].end_time);
  }

  wf << "------\n";
  for (int i = 0; i < scheduled_processor_counts.size(); i++)
    wf << "Task count scheduled on processor " << i + 1 << ": " << scheduled_processor_counts.at(i) << "\n";

  wf << "------\n";
  wf << "Total execution time: " << max_end_time << "\n";
  wf.close();
}

void run_heft(hc_env *hc_env, std::string write_file_path) {
  int task_count = hc_env->task_count;
  int processor_count = hc_env->processor_count;

  process* scheduled = new process[task_count];
  double* avail = new double[processor_count];
  std::fill(avail, avail + processor_count, 0.0);
  std::fill(scheduled, scheduled + task_count, process{-1, 0.0, 0.0, -1});
  std::queue<task> waiting_tasks = get_sorted_task_queue(compute_upward_ranks(hc_env));

  while (!waiting_tasks.empty()) {
    task curr_task = waiting_tasks.front();
    double min_eft = DBL_MAX;
    int min_processor_id = 0;

    for (int p = 0; p < processor_count; p++) {
      double curr_eft = eft(hc_env, avail, scheduled, curr_task.node, p);

      if (curr_eft < min_eft) {
        min_eft = curr_eft;
        min_processor_id = p;
      }
    }

    avail[min_processor_id] = min_eft;
    double start_time = min_eft - hc_env->execution_costs->get(curr_task.node, min_processor_id);
    process scheduled_process = {curr_task.node, start_time, min_eft, min_processor_id};
    scheduled[curr_task.node] = scheduled_process;
    waiting_tasks.pop();
  }

  write_results(scheduled, task_count, processor_count, write_file_path);

  delete[] avail;
  delete[] scheduled;
}

bool cmpf(double a, double b, double epsilon = 0.005f) {
  return (fabs(a - b) < epsilon);
}

void run_cpop(hc_env *hc_env, std::string write_file_path) {
  int task_count = hc_env->task_count;
  int processor_count = hc_env->processor_count;
  int end_task_node_id = task_count - 1;

  process* scheduled = new process[task_count];
  double* avail = new double[processor_count];
  std::fill(avail, avail + processor_count, 0.0);
  std::fill(scheduled, scheduled + task_count, process{-1, 0.0, 0.0, -1});
  std::vector<task> priority_list = compute_priority(hc_env);
  double cp = priority_list.at(0).rank;
  std::set<int> set;
  set.insert(0);
  int nk = 0;

  while (nk != end_task_node_id) {
    std::vector<int> succ_nk = hc_env->dag->get_successors_of(nk);
    for (int j = 0; j < succ_nk.size(); j++) {
      int nj = succ_nk.at(j);
      if (cmpf(priority_list.at(nj).rank, cp)) {
        set.insert(nj);
        nk = nj;
        break;
      }
    }
  }

  int pcp = find_pcp(hc_env, set);
  std::priority_queue<task, std::vector<task>, std::less<task>> pq;
  pq.push(priority_list.at(0));

  while (!pq.empty()) {
    task highest_priority_task = pq.top();
    int i = highest_priority_task.node;
    process scheduled_process = {i, 0, 0, 0};

    // check if the head is an unscheduled task
    if (scheduled[i].task_node_id != -1) {
      pq.pop();
      continue;
    }

    if (set.count(i)) {
      scheduled_process.end_time = eft(hc_env, avail, scheduled, i, pcp);
      scheduled_process.processor_id = pcp;
    } else {
      double min_eft = DBL_MAX;
      int min_processor_id = 0;

      for (int p = 0; p < processor_count; p++) {
        double curr_eft = eft(hc_env, avail, scheduled, i, p);

        if (curr_eft < min_eft) {
          min_eft = curr_eft;
          min_processor_id = p;
        }
      }

      scheduled_process.end_time = min_eft;
      scheduled_process.processor_id = min_processor_id;
    }

    avail[scheduled_process.processor_id] = scheduled_process.end_time;
    scheduled_process.start_time = scheduled_process.end_time - hc_env->execution_costs->get(i, scheduled_process.processor_id);
    scheduled[i] = scheduled_process;
    pq.pop();

    std::vector<int> succ_i = hc_env->dag->get_successors_of(i);
    for (int n = 0; n < succ_i.size(); n++) {
      int j = succ_i.at(n);
      if (scheduled[j].task_node_id == -1 && is_task_ready(hc_env, j, scheduled)) {
        pq.push(priority_list.at(j));
      }
    }
  }

  write_results(scheduled, task_count, processor_count, write_file_path);

  delete[] scheduled;
  delete[] avail;
}

