#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <float.h>
#include <algorithm>

// TODO: i should also add implementation of communication startup costs L perhaps...

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

// graph class
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

// matrix class
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


// main stuff
enum AlgorithmId {
  heft = 1,
  cpop = 2
};

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
    if (i == hc_env->processor_count - 1)
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

void run_heft(hc_env *hc_env);
void run_cpop(hc_env *hc_env);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: ./<executable> A /path-to-config" << std::endl;
    std::cout << "A: 1 - HEFT, 2 - CPOP" << std::endl;
    std::cout << "Refer to README on how to format config" << std::endl;
    return 0;
  }

  int algorithm_id = atoi(argv[1]);
  hc_env *hc_env = hc_env::init_env_from_config(argv[2]);

  switch (algorithm_id) {
    case heft:
      run_heft(hc_env);
      break;
    case cpop:
      run_cpop(hc_env);
    default:
      break;
  }

  delete hc_env;

  return 0;
}

// TODO:
// implement EST function
// implement EFT function
// implement rank_d function: DONE
// implement rank_u function: DONE

struct task {
  double rank;
  int node;
  bool operator > (const task& other_task) const {
    return (rank > other_task.rank);
  }
};

struct process {
  int task_node_id;
  //double start_time;
  double end_time;
  int processor_id;
};

std::queue<task> convert_task_list_to_queue(std::vector<task> list) {
  std::queue<task> q;
  for (int i = 0; i < list.size(); i++)
    q.push(list.at(i));
  return q;
}

std::queue<task> make_sorted_upward_task_queue(hc_env *hc_env) {
  std::vector<task> list;
  int task_count = hc_env->task_count;
  for (int i = task_count - 1; i >= 0; i--) {
    task task_i = {rank::find_upward(hc_env, i), i};
    list.push_back(task_i);
  }
  std::sort(list.begin(), list.end(), std::greater<task>());
  return convert_task_list_to_queue(list);
}

double est(hc_env *hc_env, double *avail, process *scheduled, int i, int p) {
  if (i == 0) {
    return 0;
  }

  double max = 0;
  std::vector<int> pred_i = hc_env->dag->get_predecessors_of(i);
  for (int n = 0; n < pred_i.size(); n++) {
    int j = pred_i.at(n);
    double aft = scheduled[j].end_time;
    double transfer_time = rank::get_communication_cost(hc_env, j, i, scheduled[j].processor_id, p);
    max = std::max(max, aft + transfer_time);
  }

  return std::max(max, avail[p]);
}

double eft(hc_env *hc_env, double *avail, process *scheduled, int i, int p) {
  return (double)hc_env->execution_costs->get(i, p) + est(hc_env, avail, scheduled, i, p);
}

void run_heft(hc_env *hc_env) {
  process* scheduled = new process[hc_env->task_count];
  double* avail = new double[hc_env->processor_count];
  std::fill(avail, avail + hc_env->processor_count, 0.0);
  std::queue<task> waiting_tasks = make_sorted_upward_task_queue(hc_env);

  while (!waiting_tasks.empty()) {
    task curr_task = waiting_tasks.front();
    std::cout << "Scheduling task " << curr_task.node << "..." << std::endl;
    double min_eft = DBL_MAX;
    int min_processor_id = 0;

    for (int p = 0; p < hc_env->processor_count; p++) {
      double curr_eft = eft(hc_env, avail, scheduled, curr_task.node, p);

      if (curr_eft < min_eft) {
        min_eft = curr_eft;
        min_processor_id = p;
      }
    }

    avail[min_processor_id] = min_eft;
    process scheduled_process = {curr_task.node, min_eft, min_processor_id};
    scheduled[curr_task.node] = scheduled_process;
    waiting_tasks.pop();
  }

  double max_eft = -1;
  for (int i = 0; i < hc_env->task_count; i++) {
    std::cout << "Task: " << scheduled[i].task_node_id << ", finish time: " << scheduled[i].end_time << std::endl;
    max_eft = std::max(max_eft, scheduled[i].end_time);
  }

  std::cout << "End: " << max_eft << std::endl;

  delete[] avail;
  delete[] scheduled;
}

void run_cpop(hc_env *hc_env) {
  // cpop...
}

