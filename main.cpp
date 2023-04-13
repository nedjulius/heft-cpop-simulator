#include <iostream>
#include <string>
#include <fstream>
#include <vector>

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
  std::cout << "called matrix delete" << std::endl;
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
  std::cout << "called delete" << std::endl;
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

void run_heft(graph dag, matrix data, matrix transfer_rates, matrix execution_costs);
void run_cpop(graph dag, matrix data, matrix transfer_rates, matrix execution_costs);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: ./<executable> A /path-to-config" << std::endl;
    std::cout << "A: 1 - HEFT, 2 - CPOP" << std::endl;
    std::cout << "Refer to README on how to format config" << std::endl;
    return 0;
  }

  int algorithm_id = atoi(argv[1]);
  std::cout << algorithm_id << std::endl;
  // lets read from file here.
  // then I will separate this part for separation of concenrs
  hc_env *hc_env = hc_env::init_env_from_config(argv[2]);
  std::cout << hc_env->processor_count << std::endl;
  std::cout << hc_env->task_count << std::endl;
  delete hc_env;

  return 0;
}

void run_heft(graph dag, matrix data, matrix transfer_rates, matrix execution_costs) {
  // EFT - earliest finish time
  //
  // compute rank upward, starting from the end node
  // sort the nodes in a list by nonincreasing order of rank upward values
  // while there are unscheduled nodes in the list do:
  //   select the first task ni in the list and remove it
  //   assign the task ni to the processor pj that minimizes the EFT value of ni
  // end
}

void run_cpop(graph dag, matrix data, matrix transfer_rates, matrix execution_costs) {
  // cpop...
}

