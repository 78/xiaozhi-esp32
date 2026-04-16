#!/usr/bin/env python3
"""
Sample Python project to demonstrate context extraction.
This file serves as an example for testing the extractor.
"""

import os
import sys
from typing import List, Dict, Optional
from dataclasses import dataclass


@dataclass
class User:
    """Represents a user in the system."""
    id: int
    name: str
    email: str
    active: bool = True


class UserService:
    """Service for managing users."""
    
    def __init__(self, database_url: str):
        """
        Initialize the user service.
        
        Args:
            database_url: Connection string for the database
        """
        self.database_url = database_url
        self.users: Dict[int, User] = {}
    
    def create_user(self, name: str, email: str) -> User:
        """
        Create a new user.
        
        Args:
            name: User's full name
            email: User's email address
            
        Returns:
            The newly created User object
        """
        user_id = len(self.users) + 1
        user = User(id=user_id, name=name, email=email)
        self.users[user_id] = user
        return user
    
    def get_user(self, user_id: int) -> Optional[User]:
        """
        Retrieve a user by ID.
        
        Args:
            user_id: The user's ID
            
        Returns:
            User object if found, None otherwise
        """
        return self.users.get(user_id)
    
    def list_users(self, active_only: bool = False) -> List[User]:
        """
        List all users.
        
        Args:
            active_only: If True, only return active users
            
        Returns:
            List of User objects
        """
        users = list(self.users.values())
        if active_only:
            users = [u for u in users if u.active]
        return users
    
    def deactivate_user(self, user_id: int) -> bool:
        """
        Deactivate a user.
        
        Args:
            user_id: The user's ID
            
        Returns:
            True if successful, False if user not found
        """
        user = self.users.get(user_id)
        if user:
            user.active = False
            return True
        return False


class AuthenticationService:
    """Service for handling authentication."""
    
    def __init__(self, user_service: UserService):
        """
        Initialize authentication service.
        
        Args:
            user_service: UserService instance for user lookup
        """
        self.user_service = user_service
        self.sessions: Dict[str, int] = {}
    
    def login(self, email: str, password: str) -> Optional[str]:
        """
        Authenticate a user and create session.
        
        Args:
            email: User's email
            password: User's password
            
        Returns:
            Session token if successful, None otherwise
        """
        # Simplified login logic
        users = self.user_service.list_users(active_only=True)
        for user in users:
            if user.email == email:
                session_token = f"token_{user.id}"
                self.sessions[session_token] = user.id
                return session_token
        return None
    
    def logout(self, session_token: str) -> bool:
        """
        Logout a user and destroy session.
        
        Args:
            session_token: The session token
            
        Returns:
            True if successful, False otherwise
        """
        if session_token in self.sessions:
            del self.sessions[session_token]
            return True
        return False
    
    def get_current_user(self, session_token: str) -> Optional[User]:
        """
        Get the current authenticated user.
        
        Args:
            session_token: The session token
            
        Returns:
            User object if authenticated, None otherwise
        """
        user_id = self.sessions.get(session_token)
        if user_id:
            return self.user_service.get_user(user_id)
        return None


def initialize_services(database_url: str) -> tuple[UserService, AuthenticationService]:
    """
    Initialize all services.
    
    Args:
        database_url: Database connection string
        
    Returns:
        Tuple of (UserService, AuthenticationService)
    """
    user_service = UserService(database_url)
    auth_service = AuthenticationService(user_service)
    return user_service, auth_service


def main():
    """Main entry point for the application."""
    database_url = os.environ.get('DATABASE_URL', 'sqlite:///app.db')
    
    user_service, auth_service = initialize_services(database_url)
    
    # Create some sample users
    user1 = user_service.create_user("Alice Smith", "alice@example.com")
    user2 = user_service.create_user("Bob Jones", "bob@example.com")
    
    print(f"Created users: {user1.name}, {user2.name}")
    
    # Test authentication
    token = auth_service.login("alice@example.com", "password123")
    if token:
        current_user = auth_service.get_current_user(token)
        print(f"Logged in as: {current_user.name}")
        auth_service.logout(token)
        print("Logged out successfully")


if __name__ == '__main__':
    main()
