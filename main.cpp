#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

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
    std::vector<std::vector<int>> successor_list;
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
    std::vector<int> s_edge_list;
    this->adjacency_list.push_back(edge_list);
    this->successor_list.push_back(s_edge_list);
  }
}

void graph::add_edge(int a, int b) {
  this->adjacency_list.at(a).push_back(b);
  this->successor_list.at(b).push_back(a);
}

std::vector<int> graph::get_successors_of(int i) {
  return successor_list.at(i);
}

std::vector<int> graph::get_predecessors_of(int i) {
  return adjacency_list.at(i);
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
    std::vector<int> succ_i = hc_env->dag->get_successors_of(i);
    double max = 0.0;
    for (int n = 0; n < succ_i.size(); n++) {
      int j = succ_i.at(n);
      max = std::max(max, get_avg_communication_cost(hc_env, i, j) + find_upward(hc_env, j));
    }
    return w + max;
  };

  double find_downward(hc_env *hc_env, int i) {
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
};

std::vector<task> make_sorted_upward_task_list(hc_env *hc_env) {
  std::vector<task> list;
  int task_count = hc_env->task_count;
  for (int i = task_count - 1; i >= 0; i--) {
    list.push_back(task());
    list.at(task_count - 1 - i).rank = rank::find_upward(hc_env, i);
    list.at(task_count - 1 - i).node = i;
  }
  return list;
}

void run_heft(hc_env *hc_env) {
  std::cout << "heft " << std::endl;
  std::vector<task> t_available = make_sorted_upward_task_list(hc_env);
  std::cout << "Size: " << t_available.size() << std::endl;
  // compute rank upward, starting from the end node
  // sort the nodes in a list by nonincreasing order of rank upward values
  // while there are unscheduled nodes in the list do:
  //   select the first task ni in the list and remove it
  //   assign the task ni to the processor pj that minimizes the EFT value of ni
  // end
}

void run_cpop(hc_env *hc_env) {
  std::cout << "cpop" << std::endl;
  std::cout << hc_env->processor_count << std::endl;
  for (int i = 0; i < hc_env->task_count; i++) {
    std::cout << "Rank of " << i << ": " << rank::find_downward(hc_env, i) << std::endl;
  }
  // cpop...
}

