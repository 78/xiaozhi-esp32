# Codebase Context Extractor

A powerful tool for extracting and analyzing context from large codebases. This skill helps you understand code structure, dependencies, and relationships across complex projects.

## Features

- **Multi-Language Support**: Analyze Python, JavaScript, TypeScript, Java, C#, Go, Rust, and more
- **Multiple Extraction Modes**: Full analysis, targeted extraction, dependency mapping, flow analysis
- **Smart Filtering**: Automatically excludes build artifacts, vendor code, and generated files
- **Flexible Output**: Generate Markdown, JSON, YAML, or plain text reports
- **Dependency Analysis**: Map imports, detect circular dependencies, identify unused code
- **Entity Extraction**: Find and catalog functions, classes, methods, and their relationships
- **Entry Point Detection**: Automatically identify main entry points and execution paths

## Quick Start

### Basic Usage

Extract full context from a codebase:
```bash
python context_extractor.py --target-path /path/to/project --mode full --output context.md
```

### Common Use Cases

#### 1. Understanding a New Codebase
```bash
# Get a high-level summary
python context_extractor.py --target-path ./project --mode summary

# Then get full details
python context_extractor.py --target-path ./project --mode full --output full_context.md
```

#### 2. Finding Specific Components
```bash
# Search for a class or function
python context_extractor.py --target-path ./project --mode targeted --focus "UserService"
```

#### 3. Analyzing Dependencies
```bash
# Generate dependency graph as JSON
python context_extractor.py --target-path ./project --mode dependency --format json --output deps.json
```

#### 4. Code Review Preparation
```bash
# Extract context for a specific module
python context_extractor.py --target-path ./project/module --mode full --output review_context.md
```

## Extraction Modes

### Full Mode
Performs comprehensive analysis of the entire codebase:
- File organization and structure
- All entities (classes, functions, methods)
- Import statements and dependencies
- Entry points
- Code metrics (LOC, complexity)

### Targeted Mode
Focuses on specific entities:
- Find functions/classes by name
- Show related files and dependencies
- Display signatures and documentation

### Dependency Mode
Analyzes code dependencies:
- Import/require statements
- Dependency graph (nodes and edges)
- Circular dependency detection
- External vs. internal dependencies

### Summary Mode
Quick overview without details:
- Total files and lines
- Language distribution
- Entry points
- Top-level structure

## Output Formats

### Markdown (Default)
Human-readable format with:
- Hierarchical structure
- Code formatting
- Documentation snippets
- Easy navigation

### JSON
Machine-readable format for:
- Programmatic processing
- Integration with other tools
- Data analysis

### YAML
Structured format that's:
- Human-readable
- Easy to edit
- Good for configuration

### Text
Simple plain text:
- Quick viewing
- Terminal-friendly
- Minimal formatting

## Command-Line Options

```
--target-path PATH      Path to codebase (required)
--mode MODE            Extraction mode (required)
--output FILE          Output file path (optional, defaults to stdout)
--focus QUERY          Entity to focus on (for targeted mode)
--depth N              Maximum traversal depth
--include-tests        Include test files in analysis
--language LANG        Programming language (auto-detected)
--format FORMAT        Output format: markdown, json, yaml, text
--exclude PATTERNS     Comma-separated exclusion patterns
```

## Examples

### Example 1: Analyze Python Project
```bash
python context_extractor.py \
  --target-path ~/projects/my-app \
  --mode full \
  --format markdown \
  --output analysis.md
```

### Example 2: Find All API Endpoints
```bash
python context_extractor.py \
  --target-path ~/projects/web-app \
  --mode targeted \
  --focus "api" \
  --format json
```

### Example 3: Exclude Specific Directories
```bash
python context_extractor.py \
  --target-path ~/projects/large-app \
  --mode full \
  --exclude "*/migrations/*,*/fixtures/*" \
  --output context.md
```

### Example 4: Analyze Just the Core Module
```bash
python context_extractor.py \
  --target-path ~/projects/app/core \
  --mode full \
  --include-tests \
  --output core_analysis.md
```

## Integration Examples

### Use with Code Review Tools
```bash
# Generate context for PR review
python context_extractor.py \
  --target-path ./changed-files \
  --mode full \
  --output pr_context.md
```

### Generate Documentation
```bash
# Extract API documentation
python context_extractor.py \
  --target-path ./src/api \
  --mode full \
  --format markdown \
  --output API_REFERENCE.md
```

### Dependency Audit
```bash
# Check for circular dependencies
python context_extractor.py \
  --target-path ./src \
  --mode dependency \
  --format json \
  | jq '.circular_dependencies'
```

## Language Support

### Fully Supported (AST-based analysis)
- **Python**: Complete AST parsing, docstrings, type hints
- **JavaScript/TypeScript**: ES6+ syntax, imports, exports

### Pattern-based Support
- Java, C#, Go, Rust, C/C++, Ruby, PHP, Swift, Kotlin
- Uses regex patterns for entity extraction
- Import/dependency detection

### Extensible
Add support for new languages by:
1. Creating a new analyzer class
2. Implementing `extract_imports()` and `extract_entities()`
3. Registering file extensions

## Best Practices

1. **Start Small**: Begin with summary mode before full analysis
2. **Use Exclusions**: Exclude generated code and vendor directories
3. **Focus Your Search**: Use targeted mode for specific components
4. **Export to JSON**: For programmatic processing and integration
5. **Combine Modes**: Use multiple modes for different perspectives
6. **Regular Updates**: Re-run analysis as code evolves

## Performance Tips

- Use `--exclude` to skip irrelevant directories
- Limit `--depth` for very large codebases
- Use `summary` mode for quick checks
- Target specific directories instead of entire monorepos

## Troubleshooting

### "Syntax error in file"
- File may have syntax errors
- Analyzer falls back to regex patterns
- Check the specific file manually

### "No files found"
- Check `--target-path` is correct
- Language might not be recognized
- Files might be excluded by default patterns

### Large output
- Use `--focus` to narrow scope
- Increase `--depth` limit
- Exclude test files with default settings

## Advanced Usage

### Custom Exclusion Patterns
```bash
python context_extractor.py \
  --target-path ./project \
  --mode full \
  --exclude "*/generated/*,*.pb.go,*_pb2.py"
```

### Piping to Other Tools
```bash
# Count total functions
python context_extractor.py --target-path . --mode full --format json \
  | jq '[.files[].entities[] | select(.type=="function")] | length'
```

### Batch Processing
```bash
# Analyze multiple projects
for project in ~/projects/*; do
  python context_extractor.py \
    --target-path "$project" \
    --mode summary \
    --output "${project##*/}_summary.md"
done
```

## Contributing

To extend the analyzer:
1. Add new language support in `LanguageAnalyzer` subclasses
2. Implement new extraction modes
3. Add output formatters
4. Improve entity detection patterns

## License

This skill is part of the codebase analysis toolkit.
