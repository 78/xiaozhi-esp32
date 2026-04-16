#!/bin/bash
# Example usage scripts for Codebase Context Extractor

EXTRACTOR="python ../context_extractor.py"

echo "=== Codebase Context Extractor Examples ==="
echo ""

# Example 1: Full analysis of current directory
echo "Example 1: Full analysis"
$EXTRACTOR --target-path . --mode full --output full_analysis.md
echo "✓ Full analysis saved to full_analysis.md"
echo ""

# Example 2: Summary only
echo "Example 2: Quick summary"
$EXTRACTOR --target-path . --mode summary --format text
echo ""

# Example 3: Find specific class
echo "Example 3: Find 'UserService' class"
$EXTRACTOR --target-path . --mode targeted --focus "UserService" --format json
echo ""

# Example 4: Dependency analysis
echo "Example 4: Dependency graph"
$EXTRACTOR --target-path . --mode dependency --format json --output dependencies.json
echo "✓ Dependencies saved to dependencies.json"
echo ""

# Example 5: Analyze specific directory
echo "Example 5: Analyze src directory only"
$EXTRACTOR --target-path ./src --mode full --output src_context.md
echo "✓ Source context saved to src_context.md"
echo ""

# Example 6: Include tests
echo "Example 6: Analysis including tests"
$EXTRACTOR --target-path . --mode full --include-tests --output full_with_tests.md
echo "✓ Analysis with tests saved to full_with_tests.md"
echo ""

# Example 7: Custom exclusions
echo "Example 7: Custom exclusion patterns"
$EXTRACTOR --target-path . --mode full --exclude "*/migrations/*,*/fixtures/*" --output custom_analysis.md
echo "✓ Custom analysis saved to custom_analysis.md"
echo ""

echo "=== All examples completed ==="
