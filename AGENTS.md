All classes, methods and fields must have a one line /// @brief comment.
Member fields must be declared at the end of each class.
Custom data structures must have unit tests, and within the tests also verify conformance to relevant C++ concepts.
Break complex problems into simpler primitives that act as “drop in” replacements for std containers and algorithms (which internally behave differently), then use these through composition.
P.1: Express ideas directly in code
