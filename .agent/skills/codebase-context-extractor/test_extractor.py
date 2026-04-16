#!/usr/bin/env python3
"""
Test script for the Codebase Context Extractor.
Runs basic tests to verify functionality.
"""

import sys
import os
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

from context_extractor import (
    CodebaseExtractor,
    PythonAnalyzer,
    JavaScriptAnalyzer,
    OutputFormatter,
    CodebaseContext
)


def test_python_analyzer():
    """Test Python code analysis."""
    print("Testing Python Analyzer...")
    
    test_file = Path(__file__).parent / "examples" / "sample_project.py"
    analyzer = PythonAnalyzer(str(test_file))
    
    # Test imports
    imports = analyzer.extract_imports()
    assert 'os' in imports, "Should detect 'os' import"
    assert 'sys' in imports, "Should detect 'sys' import"
    print(f"  ✓ Found {len(imports)} imports")
    
    # Test entities
    entities = analyzer.extract_entities()
    entity_names = [e.name for e in entities]
    
    assert 'User' in entity_names, "Should find User class"
    assert 'UserService' in entity_names, "Should find UserService class"
    assert 'main' in entity_names, "Should find main function"
    print(f"  ✓ Found {len(entities)} entities")
    
    # Test docstrings
    docstring_count = sum(1 for e in entities if e.docstring)
    print(f"  ✓ Found {docstring_count} entities with docstrings")
    
    print("✓ Python Analyzer tests passed\n")


def test_javascript_analyzer():
    """Test JavaScript code analysis."""
    print("Testing JavaScript Analyzer...")
    
    # Create a temporary JS file
    test_content = """
import React from 'react';
const express = require('express');

class UserController {
    constructor() {
        this.users = [];
    }
    
    getUser(id) {
        return this.users.find(u => u.id === id);
    }
}

function createUser(name, email) {
    return { name, email };
}

const deleteUser = (id) => {
    console.log('Deleting user', id);
};

export default UserController;
"""
    
    test_file = Path('/tmp/test_js_file.js')
    test_file.write_text(test_content)
    
    analyzer = JavaScriptAnalyzer(str(test_file))
    
    # Test imports
    imports = analyzer.extract_imports()
    assert 'react' in imports, "Should detect React import"
    assert 'express' in imports, "Should detect Express require"
    print(f"  ✓ Found {len(imports)} imports")
    
    # Test entities
    entities = analyzer.extract_entities()
    entity_names = [e.name for e in entities]
    
    assert 'UserController' in entity_names, "Should find UserController class"
    assert 'createUser' in entity_names, "Should find createUser function"
    assert 'deleteUser' in entity_names, "Should find deleteUser arrow function"
    print(f"  ✓ Found {len(entities)} entities")
    
    # Cleanup
    test_file.unlink()
    
    print("✓ JavaScript Analyzer tests passed\n")


def test_codebase_extractor():
    """Test full codebase extraction."""
    print("Testing Codebase Extractor...")
    
    # Use examples directory as test target
    test_path = Path(__file__).parent / "examples"
    extractor = CodebaseExtractor(str(test_path))
    
    # Test file collection
    files = extractor._collect_files()
    print(f"  ✓ Found {len(files)} source files")
    
    # Test full extraction
    context = extractor.extract_full_context()
    
    assert context.total_files > 0, "Should find at least one file"
    assert context.total_lines > 0, "Should count lines"
    assert len(context.languages) > 0, "Should detect languages"
    
    print(f"  ✓ Analyzed {context.total_files} files")
    print(f"  ✓ Total lines: {context.total_lines}")
    print(f"  ✓ Languages: {', '.join(context.languages.keys())}")
    
    # Test targeted extraction
    result = extractor.extract_targeted_context("UserService")
    assert len(result['matches']) > 0, "Should find UserService"
    print(f"  ✓ Targeted search found {len(result['matches'])} matches")
    
    # Test dependency extraction
    dep_graph = extractor.extract_dependency_graph()
    assert 'nodes' in dep_graph, "Should have nodes"
    assert 'edges' in dep_graph, "Should have edges"
    print(f"  ✓ Dependency graph has {len(dep_graph['nodes'])} nodes")
    
    print("✓ Codebase Extractor tests passed\n")


def test_output_formatters():
    """Test output formatting."""
    print("Testing Output Formatters...")
    
    # Create minimal test context
    from context_extractor import FileContext, CodeEntity
    
    context = CodebaseContext(
        root_path="/test/project",
        total_files=2,
        total_lines=100,
        languages={"Python": 2},
        files=[
            FileContext(
                path="main.py",
                language="Python",
                lines_of_code=50,
                imports=["os", "sys"],
                entities=[
                    CodeEntity(
                        name="main",
                        type="function",
                        file_path="main.py",
                        line_number=10,
                        docstring="Main entry point"
                    )
                ]
            )
        ],
        dependency_graph={"main.py": ["os", "sys"]},
        entry_points=["main.py:main"]
    )
    
    formatter = OutputFormatter()
    
    # Test Markdown
    md_output = formatter.format_markdown(context)
    assert "# Codebase Context" in md_output, "Should have title"
    assert "main.py" in md_output, "Should mention file"
    print("  ✓ Markdown formatting works")
    
    # Test JSON
    json_output = formatter.format_json(context)
    assert "root_path" in json_output, "Should have root_path"
    assert "total_files" in json_output, "Should have total_files"
    print("  ✓ JSON formatting works")
    
    # Test Text
    text_output = formatter.format_text(context)
    assert "Total Files: 2" in text_output, "Should show file count"
    print("  ✓ Text formatting works")
    
    print("✓ Output Formatter tests passed\n")


def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("Running Codebase Context Extractor Tests")
    print("=" * 60)
    print()
    
    try:
        test_python_analyzer()
        test_javascript_analyzer()
        test_codebase_extractor()
        test_output_formatters()
        
        print("=" * 60)
        print("✓ ALL TESTS PASSED")
        print("=" * 60)
        return 0
    
    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        return 1
    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(run_all_tests())
