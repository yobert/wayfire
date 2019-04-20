#ifndef INPUT_INHIBIT_HPP
#define INPUT_INHIBIT_HPP

extern "C"
{
    struct wlr_input_inhibit_manager;
}

namespace wf
{
class output_t;
}

wlr_input_inhibit_manager* create_input_inhibit();

void inhibit_output(wf::output_t *output);
bool is_output_inhibited(wf::output_t *output);
void uninhibit_output(wf::output_t *output);

#endif /* end of include guard: INPUT_INHIBIT_HPP */
