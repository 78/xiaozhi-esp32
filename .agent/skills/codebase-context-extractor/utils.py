#!/usr/bin/env python3
"""
Utility functions for the Codebase Context Extractor.
"""

import os
import json
import yaml
from pathlib import Path
from typing import Dict, List, Any, Optional
import hashlib
import pickle
from datetime import datetime, timedelta


class CacheManager:
    """Manages caching of analysis results."""
    
    def __init__(self, cache_dir: str = ".context_cache", expiry_hours: int = 24):
        self.cache_dir = Path(cache_dir)
        self.cache_dir.mkdir(exist_ok=True)
        self.expiry_hours = expiry_hours
    
    def _get_cache_key(self, target_path: str, mode: str) -> str:
        """Generate cache key from target path and mode."""
        key_str = f"{target_path}:{mode}"
        return hashlib.md5(key_str.encode()).hexdigest()
    
    def _get_cache_path(self, cache_key: str) -> Path:
        """Get path to cache file."""
        return self.cache_dir / f"{cache_key}.cache"
    
    def get(self, target_path: str, mode: str) -> Optional[Any]:
        """Retrieve cached result if available and not expired."""
        cache_key = self._get_cache_key(target_path, mode)
        cache_path = self._get_cache_path(cache_key)
        
        if not cache_path.exists():
            return None
        
        # Check expiry
        mtime = datetime.fromtimestamp(cache_path.stat().st_mtime)
        if datetime.now() - mtime > timedelta(hours=self.expiry_hours):
            cache_path.unlink()
            return None
        
        try:
            with open(cache_path, 'rb') as f:
                return pickle.load(f)
        except Exception:
            return None
    
    def set(self, target_path: str, mode: str, data: Any) -> None:
        """Store result in cache."""
        cache_key = self._get_cache_key(target_path, mode)
        cache_path = self._get_cache_path(cache_key)
        
        try:
            with open(cache_path, 'wb') as f:
                pickle.dump(data, f)
        except Exception as e:
            print(f"Warning: Failed to cache result: {e}")
    
    def clear(self) -> None:
        """Clear all cached results."""
        for cache_file in self.cache_dir.glob("*.cache"):
            cache_file.unlink()


class ConfigLoader:
    """Loads and manages configuration."""
    
    DEFAULT_CONFIG = {
        'default_mode': 'full',
        'default_format': 'markdown',
        'include_tests': False,
        'exclude_patterns': [],
    }
    
    @staticmethod
    def load(config_path: Optional[str] = None) -> Dict[str, Any]:
        """Load configuration from file or use defaults."""
        config = ConfigLoader.DEFAULT_CONFIG.copy()
        
        if config_path and Path(config_path).exists():
            with open(config_path, 'r') as f:
                if config_path.endswith('.json'):
                    user_config = json.load(f)
                else:
                    user_config = yaml.safe_load(f)
                config.update(user_config)
        
        return config


class MetricsCalculator:
    """Calculate code metrics."""
    
    @staticmethod
    def calculate_complexity(content: str) -> int:
        """
        Calculate cyclomatic complexity (simplified).
        Counts decision points in code.
        """
        complexity = 1  # Base complexity
        
        # Decision keywords that increase complexity
        keywords = [
            'if', 'elif', 'else',
            'for', 'while',
            'and', 'or',
            'case', 'when',
            '?',  # Ternary operator
        ]
        
        for keyword in keywords:
            complexity += content.count(keyword)
        
        return complexity
    
    @staticmethod
    def calculate_maintainability_index(loc: int, complexity: int, 
                                       comment_lines: int) -> float:
        """
        Calculate maintainability index (simplified version).
        Returns a score from 0-100, higher is better.
        """
        # Simplified formula
        volume = loc * 2.5  # Approximation
        
        if volume == 0 or loc == 0:
            return 100.0
        
        mi = 171 - 5.2 * (volume ** 0.5) - 0.23 * complexity - 16.2 * (comment_lines / loc)
        
        # Normalize to 0-100
        mi = max(0, min(100, mi))
        
        return mi
    
    @staticmethod
    def count_comment_lines(content: str, language: str) -> int:
        """Count comment lines in code."""
        lines = content.splitlines()
        comment_count = 0
        
        if language == 'Python':
            for line in lines:
                stripped = line.strip()
                if stripped.startswith('#'):
                    comment_count += 1
        elif language in ['JavaScript', 'TypeScript', 'Java', 'C', 'C++', 'C#', 'Go', 'Rust']:
            for line in lines:
                stripped = line.strip()
                if stripped.startswith('//'):
                    comment_count += 1
        
        # TODO: Handle multi-line comments
        
        return comment_count


class DependencyResolver:
    """Resolve and analyze dependencies."""
    
    @staticmethod
    def resolve_import_path(import_str: str, file_path: str, 
                           root_path: str) -> Optional[str]:
        """
        Resolve an import statement to actual file path.
        
        Args:
            import_str: The import string (e.g., "./utils")
            file_path: Path of the file containing the import
            root_path: Root path of the project
            
        Returns:
            Resolved file path if found, None otherwise
        """
        file_path = Path(file_path)
        root_path = Path(root_path)
        
        # Handle relative imports
        if import_str.startswith('.'):
            base_dir = file_path.parent
            import_path = (base_dir / import_str).resolve()
            
            # Try different extensions
            for ext in ['.py', '.js', '.ts', '/index.js', '/index.ts']:
                candidate = Path(str(import_path) + ext)
                if candidate.exists() and candidate.is_relative_to(root_path):
                    return str(candidate.relative_to(root_path))
        
        # Handle absolute imports (simplified)
        # This would need more sophisticated logic for real projects
        
        return None
    
    @staticmethod
    def find_circular_dependencies(graph: Dict[str, List[str]]) -> List[List[str]]:
        """
        Find circular dependencies in a dependency graph.
        
        Args:
            graph: Dependency graph as dict of node -> [dependencies]
            
        Returns:
            List of circular dependency chains
        """
        circles = []
        visited = set()
        
        def dfs(node: str, path: List[str]) -> None:
            if node in path:
                # Found a cycle
                cycle_start = path.index(node)
                cycle = path[cycle_start:]
                if cycle not in circles:
                    circles.append(cycle)
                return
            
            if node in visited:
                return
            
            visited.add(node)
            path.append(node)
            
            for neighbor in graph.get(node, []):
                dfs(neighbor, path[:])
        
        for node in graph:
            dfs(node, [])
        
        return circles
    
    @staticmethod
    def find_unused_dependencies(graph: Dict[str, List[str]], 
                                declared: List[str]) -> List[str]:
        """
        Find dependencies that are declared but not used.
        
        Args:
            graph: Actual usage graph
            declared: List of declared dependencies
            
        Returns:
            List of unused dependencies
        """
        used = set()
        for deps in graph.values():
            used.update(deps)
        
        return [dep for dep in declared if dep not in used]


class ReportGenerator:
    """Generate various reports from context data."""
    
    @staticmethod
    def generate_summary_report(context: Any) -> str:
        """Generate a summary report."""
        lines = [
            "# Summary Report",
            "",
            f"**Project**: {Path(context.root_path).name}",
            f"**Files**: {context.total_files}",
            f"**Lines of Code**: {context.total_lines:,}",
            "",
            "## Languages",
        ]
        
        for lang, count in sorted(context.languages.items(), 
                                 key=lambda x: x[1], reverse=True):
            percentage = (count / context.total_files) * 100
            lines.append(f"- {lang}: {count} files ({percentage:.1f}%)")
        
        return '\n'.join(lines)
    
    @staticmethod
    def generate_dependency_report(graph: Dict[str, Any]) -> str:
        """Generate a dependency report."""
        lines = [
            "# Dependency Report",
            "",
            f"**Total Nodes**: {len(graph.get('nodes', []))}",
            f"**Total Edges**: {len(graph.get('edges', []))}",
            "",
        ]
        
        circles = graph.get('circular_dependencies', [])
        if circles:
            lines.extend([
                "## ⚠️ Circular Dependencies Found",
                "",
            ])
            for i, circle in enumerate(circles, 1):
                lines.append(f"{i}. {' → '.join(circle)}")
            lines.append("")
        else:
            lines.append("✓ No circular dependencies detected")
            lines.append("")
        
        return '\n'.join(lines)
    
    @staticmethod
    def generate_metrics_report(files: List[Any]) -> str:
        """Generate a metrics report."""
        total_lines = sum(f.lines_of_code for f in files)
        total_entities = sum(len(f.entities) for f in files)
        
        lines = [
            "# Metrics Report",
            "",
            f"**Total Lines**: {total_lines:,}",
            f"**Total Entities**: {total_entities:,}",
            f"**Average Lines per File**: {total_lines / len(files):.1f}",
            "",
            "## Top Files by Size",
            "",
        ]
        
        sorted_files = sorted(files, key=lambda f: f.lines_of_code, reverse=True)
        for f in sorted_files[:10]:
            lines.append(f"- {f.path}: {f.lines_of_code:,} lines")
        
        return '\n'.join(lines)


def format_file_tree(root_path: str, max_depth: int = 3) -> str:
    """
    Generate a visual tree representation of files.
    
    Args:
        root_path: Root directory to start from
        max_depth: Maximum depth to traverse
        
    Returns:
        String representation of file tree
    """
    lines = [f"📁 {Path(root_path).name}"]
    
    def walk_dir(path: Path, prefix: str = "", depth: int = 0):
        if depth >= max_depth:
            return
        
        try:
            items = sorted(path.iterdir(), key=lambda x: (not x.is_dir(), x.name))
        except PermissionError:
            return
        
        for i, item in enumerate(items):
            is_last = i == len(items) - 1
            current_prefix = "└── " if is_last else "├── "
            next_prefix = "    " if is_last else "│   "
            
            icon = "📁" if item.is_dir() else "📄"
            lines.append(f"{prefix}{current_prefix}{icon} {item.name}")
            
            if item.is_dir():
                walk_dir(item, prefix + next_prefix, depth + 1)
    
    walk_dir(Path(root_path))
    
    return '\n'.join(lines)


if __name__ == '__main__':
    # Test utilities
    print("Testing utilities...")
    
    # Test cache
    cache = CacheManager('/tmp/test_cache')
    cache.set('/test/path', 'full', {'test': 'data'})
    result = cache.get('/test/path', 'full')
    assert result == {'test': 'data'}, "Cache should work"
    cache.clear()
    print("✓ Cache manager works")
    
    # Test config
    config = ConfigLoader.load()
    assert 'default_mode' in config, "Should have default config"
    print("✓ Config loader works")
    
    # Test metrics
    complexity = MetricsCalculator.calculate_complexity("if x: y elif z: w else: v")
    assert complexity > 1, "Should calculate complexity"
    print("✓ Metrics calculator works")
    
    print("\n✓ All utility tests passed!")
