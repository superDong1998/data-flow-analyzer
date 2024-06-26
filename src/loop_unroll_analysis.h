#ifndef LOOP_UNROLL_ANALYSIS_H_
#define LOOP_UNROLL_ANALYSIS_H_
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include "dbg.h"

#include "loop_mem_pat_node.h"

class ArrayPos {
private:
    int _i;
    int _j;
public:
    // ArrayPos(ArrayPos ap) {
    //     _i = ap.getI();
    //     _j = ap.getJ();
    // }

    ArrayPos(int i, int j):
        _i(i), _j(j) {}
    
    void dump() {
        std::cout << _i << "," << _j ;
    }

    int getI() {return _i;}
    int getJ() {return _j;}
};

// int convertToOffset(int i, int j, int nx, int ny) {
//     return i * ny + j;
// }

class LoopUnrollAnalysis {
private:
    // A -> offset -> position 

    LoopMemPatNode* _loop; // must be a leaf loop node

    std::unordered_map<std::string, std::unordered_map<std::string, ArrayPos*>> _w_mem_acs; 
    
    std::map<std::string, int> _cur_ind_var_value_map;

    std::vector<std::pair<ArrayPos, ArrayPos>> _intra_iter_dep;

public:
    LoopUnrollAnalysis(LoopMemPatNode* loop) :
        _loop(loop) {}


    int getPatNodeValue(PatNode* pn) {
        auto children = pn->getChildren();
        std::vector<int> child_values;
        for (auto child: children) {
            int child_value = getPatNodeValue(child);
            child_values.push_back(child_value);
        }
        auto type = pn->getType();
        if (type == CONSTANT) {
            auto const_num = pn->getConstantNum();
            // dbg(const_num);
            int const_value;
            // if (const_num) {
                const_value = stoi(pn->getConstantNum());
            // } else {
                // const_value = -1111;
            // }
            return const_value;
        } else if (type == LOOP_IND_VAR) {
            std::string ind_var = std::string(pn->getValueName());
            int ind_var_value = _cur_ind_var_value_map[ind_var];
            return ind_var_value;
        } else if (type == BIN_OP) {
            auto op = pn->getOp();
            if (op.compare("+") == 0) {
                int sum = 0;
                for (auto child_value: child_values) {
                    sum += child_value;
                }
                return sum;
            }
        } else if (type == OTHERS) {
            std::string othrs_value = std::string(pn->getValueName());
            return 0;
        } 
        return -1000;
    }

    std::string convertToOffset(PatNode* pn) {
        auto children = pn->getChildren();
        std::string offset;
        for (auto child: children) {
            offset += "[";
            auto child_value = getPatNodeValue(child);
            offset += std::to_string(child_value);
            offset += "]";
        }
        // dbg(offset);
        return offset;
    }

    void checkDependence() {
        auto children = _loop->getChildren();

        // get all loops
        std::vector<LoopMemPatNode*> loop_stack;
        loop_stack.push_back(_loop);
        auto parent = _loop->getParent();
        while (parent) {
            if (parent->getType() == LOOP_NODE) {
                loop_stack.push_back(parent);
            }
            parent = parent->getParent();
        }
        // if (loop_stack.size() == 1){
        //     // single loop level: repush and analyze this single loop
        //     loop_stack.push_back(_loop);
        // }
        if (!loop_stack[1] || !loop_stack[0]) {return;}
        auto outer_loop_pat = loop_stack[1]->getLoopPat();
        auto inner_loop_pat = loop_stack[0]->getLoopPat();
        
        int outer_start = outer_loop_pat->getStartVal();
        int outer_end = outer_loop_pat->getEndVal();
        int outer_step = outer_loop_pat->getStepVal();

        int inner_start = inner_loop_pat->getStartVal();
        int inner_end = inner_loop_pat->getEndVal();
        int inner_step = inner_loop_pat->getStepVal();

        std::string outer_ind_var = std::string(outer_loop_pat->getIndVar());
        std::string inner_ind_var = std::string(inner_loop_pat->getIndVar());


        for (int i = outer_start; i < outer_end; i += outer_step) {
            _cur_ind_var_value_map[outer_ind_var] = i;
            for (int j = inner_start; j < inner_end; j += inner_step){
                _cur_ind_var_value_map[inner_ind_var] = j;
                for (auto child: children) {
                    if (child->getType() != MEM_ACS_NODE) {
                        continue;
                    }
                    

                    auto mem_acs_pat = child->getMemAcsPat();
                    auto mode = mem_acs_pat->getAccessMode();
                    if (mode == READ) {
                        auto mem_acs_pat_node = mem_acs_pat->getPatNode();
                        std::string object_name = std::string(mem_acs_pat_node->getValueName());
                        std::string offset = convertToOffset(mem_acs_pat_node);
                        // dbg(offset);
                        // std::cout << "R:" << offset << " at " ;

                        if (_w_mem_acs.find(object_name) != _w_mem_acs.end()) {
                            auto& tmp_map = _w_mem_acs[object_name];

                            if (tmp_map.find(offset) != tmp_map.end()) {
                                std::cout << "[["<< i << "," << j << "],[";
                                auto array_pos = tmp_map[offset];
                                array_pos->dump();
                                std::cout <<"]],"<< std::endl;
                            }
                        }
                        // std::cout << std::endl;
                    }
                }

                for (auto child: children) {
                    if (child->getType() != MEM_ACS_NODE) {
                        continue;
                    }
                    auto mem_acs_pat = child->getMemAcsPat();
                    auto mode = mem_acs_pat->getAccessMode();
                    if (mode == WRITE) {
                        auto mem_acs_pat_node = mem_acs_pat->getPatNode();
                        std::string object_name = std::string(mem_acs_pat_node->getValueName());
                        std::string offset = convertToOffset(mem_acs_pat_node);
                        // dbg(offset);
                        // std::cout << "W:" << offset << " at " << i << "," << j << std::endl;
                        
                        ArrayPos* cur_array_pos = new ArrayPos(i,j);
                        _w_mem_acs[object_name].insert(std::make_pair<std::string, ArrayPos*>(std::move(offset), std::move(cur_array_pos)));

                    }
                }
            }
        }
    }
};

#endif