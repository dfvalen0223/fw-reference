
#pragma once // Current way, instead of #ifndef HAL_UART_HPP ... #define HAL_UART_HPP


//To allow use exact types like uint8_t or uint32_t
#include <cstdint> 

// Allow us use size_t: An unsigned integral type used to represent the sizes of objects in bytes (returned by sizeof).
// std::nullptr_t: The type of the null pointer (nullptr), to handle callback functions (pointers to functions) for UART interrupts.
#include <cstddef> 


namespace hal {

/**
 * @brief Abstract UART hardware abstraction.
 *
 * Drivers depend on this interface, not concrete hardware.
 * This enables SIL (Software-in-the-Loop) testing: the test
 * binary links against a mock implementation, not real hardware.
 *
 * ** Good practices **
 * CRTP (Curiously Recurring Template Pattern) Called F-bound polymorphism
 * - Less RAM/Flash and Speedup
 *
 * SOLID https://en.wikipedia.org/wiki/SOLID
 *	(S)ingle-responsibility principle (SRP): every class should have only one responsibility.
 *	(O)pen–closed principle (OCP): New features can be added without modifying existing code. 
 *	(L)iskov substitution principle (LSP): functions that use ptr/refs to base classes must be able to use ptr/refs of derived classes without knowing it.[
 *	(I)nterface segregation principle (ISP): No class should be forced to rely on tools or functions that it does not use. Divide and Conquer
 *	(D)ependency Inversion Principle (DIP): Enables changes to implementations without affecting fcns or Clients. Removes dependecy Ref HW.
 *	- Needs "virtual" fnc, and this creates a Memory Table, in ASM use go to and consume CLK Time.
 *
 * Design pattern: Dependency Inversion Principle (DIP).
 *   High-level modules (drivers) depend on abstractions (IUart),
 *   not on details (LPC1768 UART registers).
 */

// Global Status because probably we use on other Comms like i2c or spi
enum class Status {
    OK,
    TIMEOUT,
    BUFFER_FULL,
    BUFFER_EMPTY,
    FRAMING_ERROR,
    HW_ERROR,
};

class IUart {
public:
	// enum class force to use uint32_t
    enum class BaudRate : uint32_t {
        BAUD_9600   = 9600,
        BAUD_19200  = 19200,
        BAUD_57600  = 57600,
        BAUD_115200 = 115200,
        BAUD_230400 = 230400,
    };
    /*
	TO-DO: 
		File: lpc1768_uart.cpp
		Action: 
		#include "hal/hal_uart.hpp"

		namespace target {

		class LPC1768Uart : public hal::IUart {
		public:
		    hal::Status init(BaudRate baud) override {
		        
		        // Defensive Programming
		        switch (baud) {
		            case BaudRate::BAUD_9600:
		            case BaudRate::BAUD_19200:
		            case BaudRate::BAUD_57600:
		            case BaudRate::BAUD_115200:
		            case BaudRate::BAUD_230400:
		                break;
		            default:		               
		                return Status::HW_ERROR; 
		        }
		        ...
    */
    // virtual: First the real driver is deleted and, at the end, my default base class is deleted.
    virtual ~IUart() = default;

    /// Initialize UART peripheral with given baud rate NOTE: 8N1 format.
    /// NOTE: Special cases: init(BaudRate baud, DataBits bits, Parity parity)
    virtual Status init(BaudRate baud) = 0;

    /// Transmit `len` bytes; blocks up to `timeout_ms` milliseconds.
    virtual Status transmit(const uint8_t* data, std::size_t len,
                            uint32_t timeout_ms) = 0;

    /// Receive up to `len` bytes; blocks up to `timeout_ms`.
    virtual Status receive(uint8_t* buf, std::size_t len,
                           uint32_t timeout_ms) = 0;

    /// Non-blocking: number of bytes available in RX buffer.
    virtual std::size_t bytes_available() const = 0;
};

}  // namespace hal
