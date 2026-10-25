#include "error.hpp"
#include <iostream>

#include <stdexcept>


namespace cgp
{

std::string index_out_of_bounds_message(long long index, long long axis_size, std::string const& axis_label, std::string const& container_type)
{
    std::string const idx = std::to_string(index);
    std::string const N   = std::to_string(axis_size);

    std::string msg = "\n";
    msg += "\t> Out-of-bounds access on container " + container_type + "\n";
    msg += "\t>    - " + axis_label + " = " + idx + " is outside the valid range [0, " + std::to_string(axis_size - 1) + "]";
    msg += "  (axis size = " + N + ")\n";

    if (index < 0)
    {
        msg += "\t> A container cannot be accessed with a negative index.\n";
        msg += "\t  Help: contrary to Python, a negative index in C++ does not target the end of the container.\n";
    }
    else if (axis_size == 0)
    {
        msg += "\t> The container is empty: its elements cannot be accessed.\n";
        msg += "\t  Help: you may need to resize() / initialize the container before accessing it.\n";
    }
    else
    {
        msg += "\t> The index reached or exceeded the size of the container.\n";
        msg += "\t  Help: indices start at 0; a typical loop runs from index=0 while index<size().\n";
    }

    msg += "\t  (The function and variable that generated this error can be found in the Call Stack.)\n\n";
    return msg;
}

static std::map<std::string, int> warning_storage;
int cgp_warning::max_warning = 4;

void call_error(std::string const& assert_arg, std::string const& message, std::string const& filename, std::string const& function_name, int line)
{
    std::string msg = "";

    msg += "\n===============\n";
    msg += "ERROR detected !\n\n";
    msg += "Error found at this point of the code:\n";
    msg += "-------------------------------------\n";
    msg += "  - File: "+filename+"\n";
    msg += "  - Line: "+std::to_string(line)+"\n";
    msg += "  - Function: "+function_name+"\n";
    if(assert_arg!="")
        msg += "  - Assert failed on the condition: "+assert_arg+"\n";
    if(message!="")
        msg += "  - Error message: "+message+"\n";
    msg += "\n";


#if _WIN32
    msg += "> If you run the program from Visual Studio, you can look at the Call Stack Window once the error occurs.\n";
    msg += "  Visual Studio has internal debugger from which you may debug your code.\n\n";
#endif

    msg += "\n>> Please check all the error messages above on the command line to help you understand the issue. \n\n";

    // Default behavior of error is to display the error message on cerr and abort the program
    //  This default behavior can be change to throw exception in defining CGP_ERROR_EXCEPTION
#ifdef CGP_ERROR_EXCEPTION
    throw(std::logic_error(msg));
#else
    // Check your command line to see the error message.
    std::cerr << msg;
    abort();
#endif

}

void call_warning(std::string const& message_id, std::string const& extra, std::string const& filename, std::string const& function_name, int line)
{

    cgp::warning_storage[message_id] = cgp::warning_storage[message_id] + 1;

    if (cgp::warning_storage[message_id] > cgp_warning::max_warning)
        return ;
    
    std::string output  = "[Warning cgp] "+message_id+" "+extra; 
    output += "\n  file:"+filename+", function:"+function_name+", "+std::to_string(line)+"\n";
    if (cgp::warning_storage[message_id] == cgp_warning::max_warning) {
        output += "\n(The previous warning has been displayed several times and will not be displayed anymore)";
    }

    std::cout<<output<<std::endl;
}

}





