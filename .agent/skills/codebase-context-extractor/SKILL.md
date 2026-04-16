---
name: codebase-context-extractor
description: This skill provides a comprehensive context extraction system for large codebases. It intelligently analyzes code structure, dependencies, and relationships to extract relevant context for understanding, debugging, or modifying code.
---

# Codebase Context Extractor Skill

## Overview
This skill provides a comprehensive context extraction system for large codebases. It intelligently analyzes code structure, dependencies, and relationships to extract relevant context for understanding, debugging, or modifying code.

## Trigger Words
- "extract context"
- "codebase context"
- "code context"
- "analyze codebase"
- "codebase analysis"
- "code structure"
- "dependency analysis"
- "code relationships"
- "understand codebase"
- "map codebase"

## When to Use This Skill
Use this skill when you need to:
- Understand the structure and organization of a large codebase
- Extract relevant context for a specific function, class, or module
- Analyze dependencies and relationships between code components
- Generate documentation or summaries of code sections
- Prepare context for code modifications or debugging
- Identify entry points and execution flows
- Map out API surfaces and public interfaces
- Understand data flow and state management

## Instructions

When this skill is triggered, execute the `context_extractor.py` script with appropriate parameters.

### Basic Usage
```bash
python /projects/workspace/codebase-context-extractor/context_extractor.py \
  --target-path <path_to_codebase> \
  --mode <extraction_mode> \
  --output <output_file>
```

### Extraction Modes

1. **full** - Complete codebase analysis with all components
2. **targeted** - Focus on specific files, functions, or classes
3. **dependency** - Map dependencies and imports
4. **flow** - Trace execution flows and call chains
5. **api** - Extract public interfaces and API surfaces
6. **data** - Analyze data structures and models
7. **hierarchy** - Show class hierarchies and inheritance
8. **summary** - Generate high-level overview

### Parameters

- `--target-path` (required): Path to the codebase to analyze
- `--mode` (required): Extraction mode (see above)
- `--output` (optional): Output file path (default: stdout)
- `--focus` (optional): Specific file, class, or function to focus on
- `--depth` (optional): Maximum depth for traversal (default: unlimited)
- `--include-tests` (optional): Include test files in analysis (default: false)
- `--language` (optional): Programming language (auto-detected if not specified)
- `--format` (optional): Output format (markdown, json, yaml, text) (default: markdown)
- `--exclude` (optional): Patterns to exclude (comma-separated)

### Examples

1. Full codebase analysis:
```bash
python context_extractor.py --target-path ./my-project --mode full --output context.md
```

2. Targeted analysis of a specific class:
```bash
python context_extractor.py --target-path ./my-project --mode targeted --focus "UserService" --output user_service_context.md
```

3. Dependency mapping:
```bash
python context_extractor.py --target-path ./my-project --mode dependency --format json --output dependencies.json
```

4. Execution flow analysis:
```bash
python context_extractor.py --target-path ./my-project --mode flow --focus "main" --depth 5
```

## Output Structure

The extractor generates structured output including:

### For Full/Targeted Mode
- **Project Overview**: Language, structure, entry points
- **File Organization**: Directory structure and file purposes
- **Key Components**: Important classes, functions, modules
- **Dependencies**: External and internal dependencies
- **Code Metrics**: Lines of code, complexity estimates
- **Context Summary**: High-level understanding

### For Dependency Mode
- **Dependency Graph**: Visual representation of dependencies
- **Import Analysis**: All imports and their usage
- **Circular Dependencies**: Detection and reporting
- **Unused Dependencies**: Potential cleanup targets

### For Flow Mode
- **Call Chains**: Function call sequences
- **Entry Points**: Main execution paths
- **Exit Points**: Return and error handling
- **Branch Analysis**: Conditional execution paths

### For API Mode
- **Public Interfaces**: Exported functions and classes
- **API Documentation**: Signatures and docstrings
- **Usage Examples**: How to use the API
- **Versioning Info**: API version and compatibility

## Advanced Features

### Smart Context Window Management
The extractor automatically manages context size to fit within LLM token limits:
- Prioritizes most relevant code sections
- Provides summaries for less critical parts
- Includes breadcrumb navigation for context

### Multi-Language Support
Supports analysis of:
- Python
- JavaScript/TypeScript
- Java
- C#
- Go
- Rust
- C/C++
- Ruby
- PHP
- And more (extensible)

### Intelligent Filtering
- Excludes generated code, build artifacts, and vendor directories
- Focuses on business logic and core functionality
- Configurable exclusion patterns

## Integration with Other Tools

The context extractor output can be used with:
- Documentation generators
- Code review tools
- Refactoring assistants
- Bug tracking systems
- Development environments

## Best Practices

1. **Start with Summary Mode**: Get a high-level overview before diving deep
2. **Use Targeted Mode for Specific Tasks**: Focus on relevant code sections
3. **Combine with Dependency Analysis**: Understand impact of changes
4. **Leverage Flow Analysis for Debugging**: Trace execution paths
5. **Regular Updates**: Re-run analysis as codebase evolves

## Notes

- Large codebases may take time to analyze
- Consider using depth limits for very large projects
- JSON output is best for programmatic processing
- Markdown output is best for human reading
- The tool respects .gitignore patterns by default
