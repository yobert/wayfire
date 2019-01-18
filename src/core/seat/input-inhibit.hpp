#ifndef INPUT_INHIBIT_HPP
#define INPUT_INHIBIT_HPP

class wayfire_output;
void inhibit_output(wayfire_output *output);
bool is_output_inhibited(wayfire_output *output);
void uninhibit_output(wayfire_output *output);

#endif /* end of include guard: INPUT_INHIBIT_HPP */
