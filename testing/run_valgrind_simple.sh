#!/bin/bash
# Simple Valgrind test - run server manually, then Ctrl+C to stop

# Change to project root directory
cd "$(dirname "$0")/.." || exit 1

echo "Building debug version..."
make clean
make debug

echo ""
echo "=========================================="
echo "Starting server with Valgrind"
echo "=========================================="
echo ""
echo "The server will start now."
echo "In another terminal, run: ./client_debug"
echo "Then perform some operations and type 'quit'"
echo "Finally, press Ctrl+C here to stop the server"
echo ""
echo "Press Enter to start..."
read

valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --log-file=reports/valgrind_report.txt \
    ./server_debug 9090

echo ""
echo "=========================================="
echo "Valgrind Report"
echo "=========================================="
echo ""

# Show summary
if [ -f reports/valgrind_report.txt ]; then
    echo "Leak Summary:"
    grep "LEAK SUMMARY" -A 5 reports/valgrind_report.txt || echo "No leak summary found"
    echo ""
    echo "Error Summary:"
    grep "ERROR SUMMARY" reports/valgrind_report.txt || echo "No error summary found"
    echo ""
    echo "Full report saved to: reports/valgrind_report.txt"
else
    echo "No report generated"
fi
