#!/usr/bin/env python3
"""
Codebase Context Extractor
A comprehensive tool for extracting and analyzing context from large codebases.
"""

import os
import sys
import argparse
import json
import yaml
from pathlib import Path
from typing import Dict, List, Set, Optional, Any, Tuple
from collections import defaultdict
from dataclasses import dataclass, asdict
import re
import ast
import fnmatch


@dataclass
class CodeEntity:
    """Represents a code entity (function, class, module, etc.)"""
    name: str
    type: str  # 'function', 'class', 'method', 'variable', 'module'
    file_path: str
    line_number: int
    docstring: Optional[str] = None
    signature: Optional[str] = None
    dependencies: List[str] = None
    
    def __post_init__(self):
        if self.dependencies is None:
            self.dependencies = []


@dataclass
class FileContext:
    """Represents context for a single file"""
    path: str
    language: str
    lines_of_code: int
    imports: List[str]
    entities: List[CodeEntity]
    summary: Optional[str] = None
    
    def __post_init__(self):
        if self.imports is None:
            self.imports = []
        if self.entities is None:
            self.entities = []


@dataclass
class CodebaseContext:
    """Represents complete codebase context"""
    root_path: str
    total_files: int
    total_lines: int
    languages: Dict[str, int]
    files: List[FileContext]
    dependency_graph: Dict[str, List[str]]
    entry_points: List[str]
    
    def __post_init__(self):
        if self.files is None:
            self.files = []
        if self.dependency_graph is None:
            self.dependency_graph = {}
        if self.entry_points is None:
            self.entry_points = []


class LanguageAnalyzer:
    """Base class for language-specific analysis"""
    
    def __init__(self, file_path: str):
        self.file_path = file_path
        self.content = self._read_file()
    
    def _read_file(self) -> str:
        """Read file content"""
        try:
            with open(self.file_path, 'r', encoding='utf-8', errors='ignore') as f:
                return f.read()
        except Exception as e:
            print(f"Error reading {self.file_path}: {e}", file=sys.stderr)
            return ""
    
    def extract_imports(self) -> List[str]:
        """Extract import statements"""
        raise NotImplementedError
    
    def extract_entities(self) -> List[CodeEntity]:
        """Extract code entities"""
        raise NotImplementedError
    
    def get_line_count(self) -> int:
        """Get line count"""
        return len(self.content.splitlines())


class PythonAnalyzer(LanguageAnalyzer):
    """Python code analyzer"""
    
    def extract_imports(self) -> List[str]:
        """Extract Python imports"""
        imports = []
        try:
            tree = ast.parse(self.content)
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    for alias in node.names:
                        imports.append(alias.name)
                elif isinstance(node, ast.ImportFrom):
                    module = node.module or ''
                    for alias in node.names:
                        imports.append(f"{module}.{alias.name}" if module else alias.name)
        except SyntaxError:
            # Fallback to regex if AST parsing fails
            import_pattern = r'(?:from\s+(\S+)\s+)?import\s+(.+)'
            for match in re.finditer(import_pattern, self.content):
                if match.group(1):
                    imports.append(match.group(1))
                imports.extend([imp.strip().split()[0] for imp in match.group(2).split(',')])
        return imports
    
    def extract_entities(self) -> List[CodeEntity]:
        """Extract Python entities (classes, functions, methods)"""
        entities = []
        try:
            tree = ast.parse(self.content)
            
            for node in ast.walk(tree):
                if isinstance(node, ast.FunctionDef):
                    entities.append(CodeEntity(
                        name=node.name,
                        type='function',
                        file_path=self.file_path,
                        line_number=node.lineno,
                        docstring=ast.get_docstring(node),
                        signature=self._get_function_signature(node)
                    ))
                elif isinstance(node, ast.ClassDef):
                    entities.append(CodeEntity(
                        name=node.name,
                        type='class',
                        file_path=self.file_path,
                        line_number=node.lineno,
                        docstring=ast.get_docstring(node),
                        signature=self._get_class_signature(node)
                    ))
                    
                    # Extract methods
                    for item in node.body:
                        if isinstance(item, ast.FunctionDef):
                            entities.append(CodeEntity(
                                name=f"{node.name}.{item.name}",
                                type='method',
                                file_path=self.file_path,
                                line_number=item.lineno,
                                docstring=ast.get_docstring(item),
                                signature=self._get_function_signature(item)
                            ))
        except SyntaxError as e:
            print(f"Syntax error in {self.file_path}: {e}", file=sys.stderr)
        
        return entities
    
    def _get_function_signature(self, node: ast.FunctionDef) -> str:
        """Get function signature as string"""
        args = []
        for arg in node.args.args:
            args.append(arg.arg)
        return f"{node.name}({', '.join(args)})"
    
    def _get_class_signature(self, node: ast.ClassDef) -> str:
        """Get class signature with base classes"""
        bases = [base.id if isinstance(base, ast.Name) else str(base) for base in node.bases]
        if bases:
            return f"class {node.name}({', '.join(bases)})"
        return f"class {node.name}"


class JavaScriptAnalyzer(LanguageAnalyzer):
    """JavaScript/TypeScript code analyzer"""
    
    def extract_imports(self) -> List[str]:
        """Extract JavaScript imports"""
        imports = []
        
        # ES6 imports
        es6_pattern = r'import\s+.*?from\s+[\'"]([^\'"]+)[\'"]'
        imports.extend(re.findall(es6_pattern, self.content))
        
        # CommonJS requires
        require_pattern = r'require\s*\(\s*[\'"]([^\'"]+)[\'"]\s*\)'
        imports.extend(re.findall(require_pattern, self.content))
        
        return imports
    
    def extract_entities(self) -> List[CodeEntity]:
        """Extract JavaScript entities"""
        entities = []
        
        # Function declarations
        func_pattern = r'function\s+(\w+)\s*\((.*?)\)'
        for match in re.finditer(func_pattern, self.content):
            line_num = self.content[:match.start()].count('\n') + 1
            entities.append(CodeEntity(
                name=match.group(1),
                type='function',
                file_path=self.file_path,
                line_number=line_num,
                signature=f"{match.group(1)}({match.group(2)})"
            ))
        
        # Class declarations
        class_pattern = r'class\s+(\w+)'
        for match in re.finditer(class_pattern, self.content):
            line_num = self.content[:match.start()].count('\n') + 1
            entities.append(CodeEntity(
                name=match.group(1),
                type='class',
                file_path=self.file_path,
                line_number=line_num,
                signature=f"class {match.group(1)}"
            ))
        
        # Arrow functions (const/let/var name = ...)
        arrow_pattern = r'(?:const|let|var)\s+(\w+)\s*=\s*(?:async\s*)?\([^)]*\)\s*=>'
        for match in re.finditer(arrow_pattern, self.content):
            line_num = self.content[:match.start()].count('\n') + 1
            entities.append(CodeEntity(
                name=match.group(1),
                type='function',
                file_path=self.file_path,
                line_number=line_num,
                signature=match.group(1)
            ))
        
        return entities


class GenericAnalyzer(LanguageAnalyzer):
    """Generic analyzer for unsupported languages"""
    
    def extract_imports(self) -> List[str]:
        """Basic import extraction"""
        imports = []
        patterns = [
            r'import\s+(.+)',
            r'#include\s+[<"](.+)[>"]',
            r'require\s+[\'"](.+)[\'"]',
            r'use\s+(.+);',
        ]
        
        for pattern in patterns:
            imports.extend(re.findall(pattern, self.content))
        
        return imports
    
    def extract_entities(self) -> List[CodeEntity]:
        """Basic entity extraction"""
        entities = []
        
        # Function-like patterns
        func_patterns = [
            r'def\s+(\w+)\s*\(',
            r'function\s+(\w+)\s*\(',
            r'fn\s+(\w+)\s*\(',
            r'func\s+(\w+)\s*\(',
        ]
        
        for pattern in func_patterns:
            for match in re.finditer(pattern, self.content):
                line_num = self.content[:match.start()].count('\n') + 1
                entities.append(CodeEntity(
                    name=match.group(1),
                    type='function',
                    file_path=self.file_path,
                    line_number=line_num
                ))
        
        # Class-like patterns
        class_patterns = [
            r'class\s+(\w+)',
            r'struct\s+(\w+)',
            r'interface\s+(\w+)',
        ]
        
        for pattern in class_patterns:
            for match in re.finditer(pattern, self.content):
                line_num = self.content[:match.start()].count('\n') + 1
                entities.append(CodeEntity(
                    name=match.group(1),
                    type='class',
                    file_path=self.file_path,
                    line_number=line_num
                ))
        
        return entities


class CodebaseExtractor:
    """Main codebase context extractor"""
    
    LANGUAGE_EXTENSIONS = {
        '.py': 'Python',
        '.js': 'JavaScript',
        '.ts': 'TypeScript',
        '.jsx': 'JavaScript',
        '.tsx': 'TypeScript',
        '.java': 'Java',
        '.cs': 'C#',
        '.go': 'Go',
        '.rs': 'Rust',
        '.c': 'C',
        '.cpp': 'C++',
        '.h': 'C/C++',
        '.hpp': 'C++',
        '.rb': 'Ruby',
        '.php': 'PHP',
        '.swift': 'Swift',
        '.kt': 'Kotlin',
    }
    
    DEFAULT_EXCLUDE_PATTERNS = [
        '*/node_modules/*',
        '*/.git/*',
        '*/__pycache__/*',
        '*/venv/*',
        '*/env/*',
        '*/dist/*',
        '*/build/*',
        '*/target/*',
        '*/.idea/*',
        '*/.vscode/*',
        '*.min.js',
        '*.min.css',
    ]
    
    def __init__(self, target_path: str, exclude_patterns: List[str] = None,
                 include_tests: bool = False):
        self.target_path = Path(target_path)
        self.exclude_patterns = self.DEFAULT_EXCLUDE_PATTERNS + (exclude_patterns or [])
        self.include_tests = include_tests
        
        if not include_tests:
            self.exclude_patterns.extend(['*/test/*', '*/tests/*', '*_test.py', '*_test.go'])
    
    def _should_exclude(self, path: Path) -> bool:
        """Check if path should be excluded"""
        path_str = str(path)
        for pattern in self.exclude_patterns:
            if fnmatch.fnmatch(path_str, pattern):
                return True
        return False
    
    def _get_language(self, file_path: Path) -> Optional[str]:
        """Detect programming language from file extension"""
        return self.LANGUAGE_EXTENSIONS.get(file_path.suffix.lower())
    
    def _get_analyzer(self, file_path: Path) -> Optional[LanguageAnalyzer]:
        """Get appropriate analyzer for file"""
        language = self._get_language(file_path)
        
        if language == 'Python':
            return PythonAnalyzer(str(file_path))
        elif language in ['JavaScript', 'TypeScript']:
            return JavaScriptAnalyzer(str(file_path))
        elif language:
            return GenericAnalyzer(str(file_path))
        
        return None
    
    def _collect_files(self) -> List[Path]:
        """Collect all relevant source files"""
        files = []
        
        if self.target_path.is_file():
            return [self.target_path]
        
        for root, dirs, filenames in os.walk(self.target_path):
            # Filter directories
            dirs[:] = [d for d in dirs if not self._should_exclude(Path(root) / d)]
            
            for filename in filenames:
                file_path = Path(root) / filename
                
                if self._should_exclude(file_path):
                    continue
                
                if self._get_language(file_path):
                    files.append(file_path)
        
        return files
    
    def extract_full_context(self) -> CodebaseContext:
        """Extract complete codebase context"""
        files = self._collect_files()
        file_contexts = []
        languages = defaultdict(int)
        total_lines = 0
        dependency_graph = defaultdict(list)
        
        print(f"Analyzing {len(files)} files...", file=sys.stderr)
        
        for i, file_path in enumerate(files, 1):
            if i % 10 == 0:
                print(f"Progress: {i}/{len(files)}", file=sys.stderr)
            
            analyzer = self._get_analyzer(file_path)
            if not analyzer:
                continue
            
            language = self._get_language(file_path)
            languages[language] += 1
            
            imports = analyzer.extract_imports()
            entities = analyzer.extract_entities()
            lines = analyzer.get_line_count()
            total_lines += lines
            
            file_context = FileContext(
                path=str(file_path.relative_to(self.target_path)),
                language=language,
                lines_of_code=lines,
                imports=imports,
                entities=entities
            )
            
            file_contexts.append(file_context)
            
            # Build dependency graph
            for imp in imports:
                dependency_graph[file_context.path].append(imp)
        
        # Detect entry points
        entry_points = self._detect_entry_points(file_contexts)
        
        return CodebaseContext(
            root_path=str(self.target_path),
            total_files=len(file_contexts),
            total_lines=total_lines,
            languages=dict(languages),
            files=file_contexts,
            dependency_graph=dict(dependency_graph),
            entry_points=entry_points
        )
    
    def _detect_entry_points(self, file_contexts: List[FileContext]) -> List[str]:
        """Detect likely entry points"""
        entry_points = []
        
        for fc in file_contexts:
            # Common entry point file names
            if any(name in fc.path.lower() for name in ['main', 'index', 'app', '__init__']):
                entry_points.append(fc.path)
                continue
            
            # Check for main function
            for entity in fc.entities:
                if entity.name in ['main', 'Main', 'run', 'start']:
                    entry_points.append(f"{fc.path}:{entity.name}")
        
        return entry_points
    
    def extract_targeted_context(self, focus: str) -> Dict[str, Any]:
        """Extract context focused on specific entity"""
        files = self._collect_files()
        results = {
            'focus': focus,
            'matches': [],
            'related_files': []
        }
        
        for file_path in files:
            analyzer = self._get_analyzer(file_path)
            if not analyzer:
                continue
            
            entities = analyzer.extract_entities()
            
            for entity in entities:
                if focus.lower() in entity.name.lower():
                    results['matches'].append({
                        'entity': asdict(entity),
                        'file': str(file_path.relative_to(self.target_path)),
                        'imports': analyzer.extract_imports()
                    })
                    results['related_files'].append(str(file_path.relative_to(self.target_path)))
        
        return results
    
    def extract_dependency_graph(self) -> Dict[str, Any]:
        """Extract dependency information"""
        context = self.extract_full_context()
        
        graph = {
            'nodes': [],
            'edges': [],
            'circular_dependencies': []
        }
        
        # Build nodes
        for file_ctx in context.files:
            graph['nodes'].append({
                'id': file_ctx.path,
                'language': file_ctx.language,
                'lines': file_ctx.lines_of_code
            })
        
        # Build edges
        for source, targets in context.dependency_graph.items():
            for target in targets:
                graph['edges'].append({
                    'from': source,
                    'to': target
                })
        
        # Detect circular dependencies
        graph['circular_dependencies'] = self._find_circular_deps(context.dependency_graph)
        
        return graph
    
    def _find_circular_deps(self, dep_graph: Dict[str, List[str]]) -> List[List[str]]:
        """Find circular dependencies"""
        circles = []
        
        def dfs(node, path, visited):
            if node in path:
                cycle_start = path.index(node)
                circles.append(path[cycle_start:])
                return
            
            if node in visited:
                return
            
            visited.add(node)
            path.append(node)
            
            for neighbor in dep_graph.get(node, []):
                dfs(neighbor, path[:], visited)
        
        for node in dep_graph:
            dfs(node, [], set())
        
        return circles


class OutputFormatter:
    """Format extraction results"""
    
    @staticmethod
    def format_markdown(context: CodebaseContext) -> str:
        """Format as Markdown"""
        lines = [
            f"# Codebase Context: {Path(context.root_path).name}",
            "",
            "## Overview",
            f"- **Total Files**: {context.total_files}",
            f"- **Total Lines**: {context.total_lines:,}",
            f"- **Languages**: {', '.join(f'{lang} ({count})' for lang, count in context.languages.items())}",
            "",
            "## Entry Points",
        ]
        
        for entry in context.entry_points:
            lines.append(f"- `{entry}`")
        
        lines.extend(["", "## File Structure", ""])
        
        # Group files by directory
        files_by_dir = defaultdict(list)
        for fc in context.files:
            dir_name = str(Path(fc.path).parent)
            files_by_dir[dir_name].append(fc)
        
        for dir_name in sorted(files_by_dir.keys()):
            lines.append(f"### {dir_name or '(root)'}")
            for fc in files_by_dir[dir_name]:
                lines.append(f"- **{Path(fc.path).name}** ({fc.language}, {fc.lines_of_code} lines)")
                
                if fc.entities:
                    entity_summary = {}
                    for entity in fc.entities:
                        entity_summary[entity.type] = entity_summary.get(entity.type, 0) + 1
                    
                    summary_str = ', '.join(f"{count} {etype}{'s' if count > 1 else ''}"
                                           for etype, count in entity_summary.items())
                    lines.append(f"  - Contains: {summary_str}")
            lines.append("")
        
        lines.extend(["## Key Components", ""])
        
        # List significant entities
        all_entities = []
        for fc in context.files:
            all_entities.extend(fc.entities)
        
        # Group by type
        by_type = defaultdict(list)
        for entity in all_entities:
            by_type[entity.type].append(entity)
        
        for entity_type in ['class', 'function', 'method']:
            if entity_type in by_type:
                lines.append(f"### {entity_type.title()}s")
                for entity in sorted(by_type[entity_type], key=lambda e: e.name)[:20]:  # Top 20
                    loc = f"{Path(entity.file_path).name}:{entity.line_number}"
                    if entity.signature:
                        lines.append(f"- `{entity.signature}` - {loc}")
                    else:
                        lines.append(f"- `{entity.name}` - {loc}")
                    
                    if entity.docstring:
                        # First line of docstring
                        first_line = entity.docstring.split('\n')[0].strip()
                        if first_line:
                            lines.append(f"  - {first_line}")
                lines.append("")
        
        lines.extend(["## Dependencies", ""])
        
        # Collect unique imports
        all_imports = set()
        for fc in context.files:
            all_imports.update(fc.imports)
        
        # Separate external and internal
        external = [imp for imp in all_imports if not imp.startswith('.')]
        
        if external:
            lines.append("### External Dependencies")
            for imp in sorted(external)[:30]:  # Top 30
                lines.append(f"- `{imp}`")
            lines.append("")
        
        return '\n'.join(lines)
    
    @staticmethod
    def format_json(data: Any) -> str:
        """Format as JSON"""
        if isinstance(data, CodebaseContext):
            data = asdict(data)
        return json.dumps(data, indent=2, default=str)
    
    @staticmethod
    def format_yaml(data: Any) -> str:
        """Format as YAML"""
        if isinstance(data, CodebaseContext):
            data = asdict(data)
        return yaml.dump(data, default_flow_style=False, sort_keys=False)
    
    @staticmethod
    def format_text(context: CodebaseContext) -> str:
        """Format as plain text"""
        lines = [
            f"Codebase Context: {Path(context.root_path).name}",
            "=" * 60,
            "",
            f"Total Files: {context.total_files}",
            f"Total Lines: {context.total_lines:,}",
            f"Languages: {', '.join(context.languages.keys())}",
            "",
            "Entry Points:",
        ]
        
        for entry in context.entry_points:
            lines.append(f"  - {entry}")
        
        lines.append("")
        lines.append("Files:")
        
        for fc in context.files:
            lines.append(f"  {fc.path} ({fc.language}, {fc.lines_of_code} lines)")
            if fc.entities:
                lines.append(f"    Entities: {len(fc.entities)}")
        
        return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Extract context from codebases',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('--target-path', required=True,
                       help='Path to the codebase to analyze')
    parser.add_argument('--mode', required=True,
                       choices=['full', 'targeted', 'dependency', 'flow', 'api', 
                               'data', 'hierarchy', 'summary'],
                       help='Extraction mode')
    parser.add_argument('--output', help='Output file path (default: stdout)')
    parser.add_argument('--focus', help='Specific file, class, or function to focus on')
    parser.add_argument('--depth', type=int, help='Maximum traversal depth')
    parser.add_argument('--include-tests', action='store_true',
                       help='Include test files in analysis')
    parser.add_argument('--language', help='Programming language (auto-detected if not specified)')
    parser.add_argument('--format', default='markdown',
                       choices=['markdown', 'json', 'yaml', 'text'],
                       help='Output format')
    parser.add_argument('--exclude', help='Patterns to exclude (comma-separated)')
    
    args = parser.parse_args()
    
    # Parse exclude patterns
    exclude_patterns = []
    if args.exclude:
        exclude_patterns = [p.strip() for p in args.exclude.split(',')]
    
    # Create extractor
    extractor = CodebaseExtractor(
        args.target_path,
        exclude_patterns=exclude_patterns,
        include_tests=args.include_tests
    )
    
    # Extract based on mode
    if args.mode == 'full':
        result = extractor.extract_full_context()
    elif args.mode == 'targeted':
        if not args.focus:
            print("Error: --focus is required for targeted mode", file=sys.stderr)
            sys.exit(1)
        result = extractor.extract_targeted_context(args.focus)
    elif args.mode == 'dependency':
        result = extractor.extract_dependency_graph()
    elif args.mode == 'summary':
        context = extractor.extract_full_context()
        # Create summary version
        context.files = context.files[:10]  # Limit to first 10 files
        result = context
    else:
        print(f"Mode '{args.mode}' not fully implemented yet", file=sys.stderr)
        sys.exit(1)
    
    # Format output
    formatter = OutputFormatter()
    
    if args.format == 'markdown':
        if isinstance(result, CodebaseContext):
            output = formatter.format_markdown(result)
        else:
            output = formatter.format_json(result)
    elif args.format == 'json':
        output = formatter.format_json(result)
    elif args.format == 'yaml':
        output = formatter.format_yaml(result)
    else:  # text
        if isinstance(result, CodebaseContext):
            output = formatter.format_text(result)
        else:
            output = json.dumps(result, indent=2, default=str)
    
    # Write output
    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(output)
        print(f"Context written to {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == '__main__':
    main()
