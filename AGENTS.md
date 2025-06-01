All classes, methods and fields must have a one line /// @brief comment.
Member fields must be declared at the end of each class.
Custom data structures must have unit tests, and within the tests also verify conformance to relevant C++ concepts.
Custom container classes must begin with a multi line doxygen comment detailing
their purpose and the standard container they mimic (as a superset or subset).
Break complex problems into simpler primitives that act as “drop in” replacements for std containers and algorithms (which internally behave differently), then use these through composition.
P.1: Express ideas directly in code
